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
 * Afters
 */

struct flt_after {
    struct flt_counter  active_count;
    struct flt_local  *shards;
    struct flt_task  *trigger;
};

struct flt_after_shard {
    struct flt_after  *after;
    size_t  count;
};

static void
flt_after_trigger__finished(struct flt *flt, struct flt_task *task, void *ud)
{
    struct flt_after  *after = ud;
    struct flt_local  *shards = after->shards;
    flt_release(flt, struct flt_after, after);
    flt_local_free(flt, shards);
}

struct flt_after *
flt_after_new(struct flt *flt, struct flt_task *trigger)
{
    unsigned int  i;
    struct flt_after  *after = flt_claim(flt, struct flt_after);
    struct flt_after_shard  *shard;
    after->trigger = trigger;
    flt_task_add_on_finished(flt, trigger, flt_after_trigger__finished, after);
    after->shards = flt_local_new(flt, struct flt_after_shard);
    flt_local_foreach(flt, struct flt_after_shard, after->shards, i, shard) {
        shard->after = after;
        shard->count = 0;
    }
    flt_counter_init(&after->active_count);
    return flt_finish_claim(flt, struct flt_after, after);
}

struct flt_after_shard *
flt_after_get_shard(struct flt *flt, struct flt_after *after,
                    unsigned int index)
{
    return flt_local_get_shard_typed
        (flt, struct flt_after_shard, after->shards, index);
}

static void
flt_after_tracked__finished(struct flt *flt, struct flt_task *task, void *ud)
{
    struct flt_after  *after = ud;
    struct flt_after_shard  *shard =
        flt_local_get_current_shard_typed
        (flt, struct flt_after_shard, after->shards);

    if (--shard->count == 0) {
        if (flt_counter_dec(&after->active_count)) {
            flt_task_schedule(flt, after->trigger);
        }
    }
}

static void
flt_after_tracked__migrate(struct flt *from, struct flt *to,
                           struct flt_task *task, void *ud)
{
    struct flt_after  *after = ud;
    struct flt_after_shard  *from_shard =
        flt_local_get_current_shard_typed
        (from, struct flt_after_shard, after->shards);
    struct flt_after_shard  *to_shard =
        flt_local_get_current_shard_typed
        (to, struct flt_after_shard, after->shards);

    from_shard->count--;
    to_shard->count++;

    if (to_shard->count == 1) {
        if (from_shard->count == 0) {
            /* The from context just became inactive and the to context just
             * became active with this migration.  The net result is that the
             * total number of active contexts didn't change, so we don't need
             * to bump the global after. */
        } else {
            flt_counter_inc(&after->active_count);
        }
    } else if (from_shard->count == 0) {
        (void) flt_counter_dec(&after->active_count);
    }
}

void
flt_after_track(struct flt *flt, struct flt_after *after,
                struct flt_task *task)
{
    struct flt_after_shard  *shard =
        flt_local_get_current_shard_typed
        (flt, struct flt_after_shard, after->shards);
    if (shard->count++ == 0) {
        flt_counter_inc(&after->active_count);
    }
    flt_task_add_on_finished(flt, task, flt_after_tracked__finished, after);
    flt_task_add_on_migrate(flt, task, flt_after_tracked__migrate, after);
}

void
flt_after_shard_track(struct flt *flt, struct flt_after_shard *shard,
                      struct flt_task *task)
{
    struct flt_after  *after = shard->after;
    if (shard->count++ == 0) {
        flt_counter_inc(&after->active_count);
    }
    flt_task_add_on_finished(flt, task, flt_after_tracked__finished, after);
    flt_task_add_on_migrate(flt, task, flt_after_tracked__migrate, after);
}
