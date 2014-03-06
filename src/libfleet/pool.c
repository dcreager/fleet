/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdlib.h>

#include "libcork/core.h"
#include "libcork/ds.h"

#include "fleet.h"


/*-----------------------------------------------------------------------
 * Memory pools
 */

#define BATCH_COUNT  128

struct flt_pool {
    struct flt_local  *pool_ctxs;
    size_t  instance_size;
    void  *ud;
    flt_pool_init_f  *init;
    flt_pool_init_f  *reuse;
    flt_pool_done_f  *done;
};

struct flt_pool_ctx {
    struct flt_pool  *pool;
    void  *unused;
};

static void
flt_pool_ctx__init(struct flt *flt, void *ud, void *instance,
                   unsigned int index)
{
    struct flt_pool  *pool = ud;
    struct flt_pool_ctx  *pool_ctx = instance;
    pool_ctx->pool = pool;
    pool_ctx->unused = NULL;
}

static void
flt_pool_ctx__done(struct flt *flt, void *ud, void *instance,
                   unsigned int index)
{
    struct flt_pool  *pool = ud;
    struct flt_pool_ctx  *pool_ctx = instance;
    void  *curr;
    void  *next;
    for (curr = pool_ctx->unused; curr != NULL; curr = next) {
        void  **ptr = curr;
        pool->done(flt, pool->ud, curr);
        next = *ptr;
        free(curr);
    }
}

struct flt_pool *
flt_pool_new_size(struct flt *flt, size_t instance_size, void *ud,
                  flt_pool_init_f *init,
                  flt_pool_init_f *reuse,
                  flt_pool_done_f *done)
{
    struct flt_pool  *pool = cork_new(struct flt_pool);
    pool->pool_ctxs = flt_local_new
        (flt, struct flt_pool_ctx, pool,
         flt_pool_ctx__init, flt_pool_ctx__done);
    pool->instance_size =
        instance_size < sizeof(void *)? sizeof(void *): instance_size;
    pool->ud = ud;
    pool->init = init;
    pool->reuse = reuse;
    pool->done = done;
    return pool;
}

void
flt_pool_free(struct flt *flt, struct flt_pool *pool)
{
    flt_local_free(flt, pool->pool_ctxs);
    free(pool);
}

struct flt_pool_ctx *
flt_pool_get(struct flt *flt, struct flt_pool *pool)
{
    return flt_local_get(flt, pool->pool_ctxs);
}

struct flt_pool_ctx *
flt_pool_get_index(struct flt *flt, struct flt_pool *pool, unsigned int index)
{
    return flt_local_get_index(flt, pool->pool_ctxs, index);
}

struct flt_pool_ctx *
flt_pool_ctx_get_index(struct flt *flt, struct flt_pool_ctx *pool_ctx,
                       unsigned int index)
{
    return flt_local_ctx_get_index(flt, pool_ctx, index);
}

CORK_ATTR_NOINLINE
static void *
flt_pool_ctx_create_new(struct flt *flt, struct flt_pool *pool)
{
    void  *instance = cork_malloc(pool->instance_size);
    return pool->init(flt, pool->ud, instance);
}

void *
flt_pool_ctx_new_instance(struct flt *flt, struct flt_pool_ctx *pool_ctx)
{
    struct flt_pool  *pool = pool_ctx->pool;
    if (CORK_LIKELY(pool_ctx->unused != NULL)) {
        void  *instance = pool_ctx->unused;
        void  **ptr = instance;
        pool_ctx->unused = *ptr;
        return pool->reuse(flt, pool->ud, instance);
    } else {
        return flt_pool_ctx_create_new(flt, pool);
    }
}

void
flt_pool_ctx_free_instance(struct flt *flt, struct flt_pool_ctx *pool_ctx,
                           void *instance)
{
    void  **ptr = instance;
    *ptr = pool_ctx->unused;
    pool_ctx->unused = instance;
}
