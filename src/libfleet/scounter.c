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
    struct flt_local  *counter_ctxs;
};

struct flt_scounter_ctx {
    struct flt_scounter  *counter;
    size_t  count;
};

static void
flt_scounter_ctx_init(struct flt *flt, void *ud, void *vinstance,
                      unsigned int index)
{
    struct flt_scounter  *counter = ud;
    struct flt_scounter_ctx  *counter_ctx = vinstance;
    counter_ctx->counter = counter;
    counter_ctx->count = 0;
}

static void
flt_scounter_ctx_done(struct flt *flt, void *ud, void *vinstance,
                      unsigned int index)
{
}

struct flt_scounter *
flt_scounter_new(struct flt *flt)
{
    struct flt_scounter  *counter = cork_new(struct flt_scounter);
    counter->counter_ctxs = flt_local_new
        (flt, struct flt_scounter_ctx, counter,
         flt_scounter_ctx_init, flt_scounter_ctx_done);
    flt_counter_init(&counter->active_count);
    return counter;
}

static void
flt_scounter_free(struct flt *flt, struct flt_scounter *counter)
{
    flt_local_free(flt, counter->counter_ctxs);
    free(counter);
}

struct flt_scounter_ctx *
flt_scounter_get(struct flt *flt, struct flt_scounter *counter,
                 unsigned int index)
{
    return flt_local_get_index(flt, counter->counter_ctxs, index);
}

void
flt_scounter_inc(struct flt *flt, struct flt_scounter *counter)
{
    struct flt_scounter_ctx  *counter_ctx =
        flt_local_get(flt, counter->counter_ctxs);
    if (counter_ctx->count++ == 0) {
        flt_counter_inc(&counter->active_count);
    }
}

void
flt_scounter_ctx_inc(struct flt *flt, struct flt_scounter_ctx *counter_ctx)
{
    if (counter_ctx->count++ == 0) {
        flt_counter_inc(&counter_ctx->counter->active_count);
    }
}

bool
flt_scounter_ctx_dec(struct flt *flt, struct flt_scounter_ctx *counter_ctx)
{
    if (--counter_ctx->count == 0) {
        if (flt_counter_dec(&counter_ctx->counter->active_count)) {
            flt_scounter_free(flt, counter_ctx->counter);
            return true;
        }
    }
    return false;
}

struct flt_scounter_ctx *
flt_scounter_ctx_migrate(struct flt *from, struct flt *to,
                         struct flt_scounter_ctx *from_counter_ctx)
{
    struct flt_scounter  *counter = from_counter_ctx->counter;
    struct flt_scounter_ctx  *to_counter_ctx =
        flt_local_ctx_migrate(from, to, from_counter_ctx);

    from_counter_ctx->count--;
    to_counter_ctx->count++;

    if (to_counter_ctx->count == 1) {
        if (from_counter_ctx->count == 0) {
            /* The from context just became inactive and the to context just
             * became active with this migration.  The net result is that the
             * total number of active contexts didn't change, so we don't need
             * to bump the global counter. */
        } else {
            flt_counter_inc(&counter->active_count);
        }
    } else if (from_counter_ctx->count == 0) {
        (void) flt_counter_dec(&counter->active_count);
    }

    return to_counter_ctx;
}
