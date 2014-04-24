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

static void
flt_scounter_shard_init(struct flt *flt, void *ud, void *vinstance,
                        unsigned int index)
{
    struct flt_scounter  *counter = ud;
    struct flt_scounter_shard  *shard = vinstance;
    shard->counter = counter;
    shard->count = 0;
}

static void
flt_scounter_shard_done(struct flt *flt, void *ud, void *vinstance,
                        unsigned int index)
{
}

struct flt_scounter *
flt_scounter_new(struct flt *flt)
{
    struct flt_scounter  *counter = cork_new(struct flt_scounter);
    counter->shards = flt_local_new
        (flt, struct flt_scounter_shard, counter,
         flt_scounter_shard_init, flt_scounter_shard_done);
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
    return flt_local_get_index(flt, counter->shards, index);
}

void
flt_scounter_inc(struct flt *flt, struct flt_scounter *counter)
{
    struct flt_scounter_shard  *shard = flt_local_get(flt, counter->shards);
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

struct flt_scounter_shard *
flt_scounter_shard_migrate(struct flt *from, struct flt *to,
                           struct flt_scounter_shard *from_shard)
{
    struct flt_scounter  *counter = from_shard->counter;
    struct flt_scounter_shard  *to_shard =
        flt_local_shard_migrate(from, to, from_shard);

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
