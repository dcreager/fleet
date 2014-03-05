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
    struct flt_local  *after_ctxs;
    struct flt_task  *task;
};

struct flt_after_ctx {
    struct flt_after  *after;
    size_t  count;
};

static void
flt_after_ctx__init(struct flt *flt, void *ud, void *vinstance,
                    unsigned int index)
{
    struct flt_after  *after = ud;
    struct flt_after_ctx  *after_ctx = vinstance;
    after_ctx->after = after;
    after_ctx->count = 0;
}

static void
flt_after_ctx__done(struct flt *flt, void *ud, void *vinstance,
                    unsigned int index)
{
}

struct flt_after *
flt_after_new(struct flt *flt)
{
    struct flt_after  *after = cork_new(struct flt_after);
    after->task = NULL;
    after->after_ctxs = flt_local_new
        (flt, struct flt_after_ctx, after,
         flt_after_ctx__init, flt_after_ctx__done);
    flt_counter_init(&after->active_count);
    return after;
}

void
flt_after_set_task(struct flt *flt, struct flt_after *after)
{
    after->task = flt_defer_task(flt);
}

static void
flt_after_free(struct flt *flt, struct flt_after *after)
{
    flt_local_free(flt, after->after_ctxs);
    free(after);
}

struct flt_after_ctx *
flt_after_get(struct flt *flt, struct flt_after *after)
{
    return flt_local_get(flt, after->after_ctxs);
}

struct flt_after_ctx *
flt_after_get_index(struct flt *flt, struct flt_after *after,
                    unsigned int index)
{
    return flt_local_get_index(flt, after->after_ctxs, index);
}

struct flt_after_ctx *
flt_after_ctx_get_index(struct flt *flt, struct flt_after_ctx *after_ctx,
                        unsigned int index)
{
    return flt_local_ctx_get_index(flt, after_ctx, index);
}

CORK_ATTR_NOINLINE
static void
flt_after_fire_task(struct flt *flt, struct flt_after *after)
{
    struct flt_task  *task = after->task;
    flt_after_free(flt, after);
    flt_run_deferred_task(flt, task);
}

static void
flt_after__run(struct flt *flt, void *ud, size_t i)
{
    struct flt_after_ctx  *after_ctx = ud;
    struct flt_after  *after = after_ctx->after;

    if (--after_ctx->count == 0) {
        if (flt_counter_dec(&after->active_count)) {
            flt_after_fire_task(flt, after);
        }
    }
}

static void *
flt_after__migrate(struct flt *from, struct flt *to, void *ud, size_t i)
{
    struct flt_after_ctx  *from_after_ctx = ud;
    struct flt_after  *after = from_after_ctx->after;
    struct flt_after_ctx  *to_after_ctx =
        flt_local_ctx_migrate(from, to, from_after_ctx);

    from_after_ctx->count--;
    to_after_ctx->count++;

    if (to_after_ctx->count == 1) {
        if (from_after_ctx->count == 0) {
            /* The from context just became inactive and the to context just
             * became active with this migration.  The net result is that the
             * total number of active contexts didn't change, so we don't need
             * to bump the global after. */
        } else {
            flt_counter_inc(&after->active_count);
        }
    } else if (from_after_ctx->count == 0) {
        (void) flt_counter_dec(&after->active_count);
    }

    return to_after_ctx;
}

void
flt_after_add_step(struct flt *flt, struct flt_after *after)
{
    struct flt_after_ctx  *after_ctx = flt_local_get(flt, after->after_ctxs);
    if (after_ctx->count++ == 0) {
        flt_counter_inc(&after_ctx->after->active_count);
    }
    flt_add_post_migratable
        (flt, flt_after__run, after_ctx, 0, flt_after__migrate);
}

void
flt_after_ctx_add_step(struct flt *flt, struct flt_after_ctx *after_ctx)
{
    if (after_ctx->count++ == 0) {
        flt_counter_inc(&after_ctx->after->active_count);
    }
    flt_add_post_migratable
        (flt, flt_after__run, after_ctx, 0, flt_after__migrate);
}
