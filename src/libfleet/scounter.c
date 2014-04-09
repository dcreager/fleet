/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include "libcork/core.h"

#include "fleet.h"
#include "fleet/threads.h"


/*-----------------------------------------------------------------------
 * Semaphore counters
 */

struct flt_scounter {
    struct flt_counter  active_count;
    struct flt_local  *shards;
};

struct flt_scounter_shard {
    struct flt_scounter  *counter;
    size_t  count;
};

struct flt_scounter *
flt_scounter_new(struct flt *flt)
{
    unsigned int  i;
    struct flt_scounter  *counter = cork_new(struct flt_scounter);
    struct flt_local  *shards;
    struct flt_scounter_shard  *shard;
    shards = flt_local_new(flt, struct flt_scounter_shard);
    flt_local_foreach(flt, struct flt_scounter_shard, shards, i, shard) {
        shard->counter = counter;
        shard->count = 0;
    }
    counter->shards = shards;
    flt_counter_init(&counter->active_count);
    return counter;
}

static void
flt_scounter_free(struct flt *flt, struct flt_scounter *counter)
{
    flt_local_free(flt, counter->shards);
    free(counter);
}

struct flt_scounter_shard *
flt_scounter_get(struct flt *flt, struct flt_scounter *counter,
                 unsigned int index)
{
    return flt_local_get_shard_typed
        (flt, struct flt_scounter_shard, counter->shards, index);
}

void
flt_scounter_inc(struct flt *flt, struct flt_scounter *counter)
{
    struct flt_scounter_shard  *shard =
        flt_local_get_current_shard_typed
        (flt, struct flt_scounter_shard, counter->shards);
    if (shard->count++ == 0) {
        flt_counter_inc(&counter->active_count);
    }
}

void
flt_scounter_shard_inc(struct flt *flt, struct flt_scounter_shard *shard)
{
    if (shard->count++ == 0) {
        flt_counter_inc(&shard->counter->active_count);
    }
}

bool
flt_scounter_dec(struct flt *flt, struct flt_scounter *counter)
{
    struct flt_scounter_shard  *shard =
        flt_local_get_current_shard_typed
        (flt, struct flt_scounter_shard, counter->shards);
    if (--shard->count == 0) {
        if (flt_counter_dec(&counter->active_count)) {
            flt_scounter_free(flt, counter);
            return true;
        }
    }
    return false;
}

bool
flt_scounter_shard_dec(struct flt *flt, struct flt_scounter_shard *shard)
{
    if (--shard->count == 0) {
        if (flt_counter_dec(&shard->counter->active_count)) {
            flt_scounter_free(flt, shard->counter);
            return true;
        }
    }
    return false;
}

void
flt_scounter_migrate(struct flt *from, struct flt *to,
                     struct flt_scounter *counter)
{
    struct flt_scounter_shard  *from_shard =
        flt_local_get_current_shard_typed
        (from, struct flt_scounter_shard, counter->shards);
    struct flt_scounter_shard  *to_shard =
        flt_local_get_current_shard_typed
        (to, struct flt_scounter_shard, counter->shards);

    from_shard->count--;
    to_shard->count++;

    if (to_shard->count == 1) {
        if (from_shard->count == 0) {
            /* The from context just became inactive and the to context just
             * became active with this migration.  The net result is that the
             * total number of active contexts didn't change, so we don't need
             * to bump the global counter. */
        } else {
            flt_counter_inc(&counter->active_count);
        }
    } else if (from_shard->count == 0) {
        (void) flt_counter_dec(&counter->active_count);
    }
}

struct flt_scounter_shard *
flt_scounter_shard_migrate(struct flt *from, struct flt *to,
                           struct flt_scounter_shard *from_shard)
{
    struct flt_scounter  *counter = from_shard->counter;
    struct flt_scounter_shard  *to_shard =
        flt_local_shard_migrate_typed
        (from, struct flt_scounter_shard, from_shard, to);

    from_shard->count--;
    to_shard->count++;

    if (to_shard->count == 1) {
        if (from_shard->count == 0) {
            /* The from context just became inactive and the to context just
             * became active with this migration.  The net result is that the
             * total number of active contexts didn't change, so we don't need
             * to bump the global counter. */
        } else {
            flt_counter_inc(&counter->active_count);
        }
    } else if (from_shard->count == 0) {
        (void) flt_counter_dec(&counter->active_count);
    }

    return to_shard;
}
