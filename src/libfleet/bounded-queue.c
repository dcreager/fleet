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


/*-----------------------------------------------------------------------
 * Bounded queues
 */

#define DEFAULT_BATCH_SIZE  128

struct flt_bqueue_batch_priv {
    struct flt_bqueue_batch  public;
};

struct flt_bqueue {
    size_t  batch_count;
    size_t  batch_size;
};

struct flt_bqueue *
flt_bqueue_new(struct flt *flt, struct flt_allocator *alloc,
               flt_task *func, void *ud)
{
    struct flt_bqueue  *queue = cork_new(struct flt_bqueue);
    /* The default is to create two batches for each execution context. */
    queue->batch_count = flt->count * 2;
    queue->batch_size = DEFAULT_BATCH_SIZE;
    return queue;
}

void
flt_bqueue_free(struct flt *flt, struct flt_bqueue *queue)
{
    free(queue);
}

void
flt_bqueue_set_minimum_batch_count(struct flt *flt, struct flt_bqueue *queue,
                                   size_t minimum)
{
    if (minimum > queue->batch_count) {
        queue->batch_count = minimum;
    }
}

void
flt_bqueue_set_minimum_batch_size(struct flt *flt, struct flt_bqueue *queue,
                                  size_t minimum)
{
    if (minimum > queue->batch_size) {
        queue->batch_count = minimum;
    }
}

void
flt_bqueue_start(struct flt *flt, struct flt_bqueue *queue)
{
}

void
flt_bqueue_stop(struct flt *flt, struct flt_bqueue *queue)
{
}
