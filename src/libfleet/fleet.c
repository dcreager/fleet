/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdlib.h>

#include "fleet.h"
#include "fleet/internal.h"
#include "fleet/task.h"


/*-----------------------------------------------------------------------
 * Tasks
 */

/* 8Kb batches */
#define TASK_BATCH_SIZE  8192
#define TASK_BATCH_COUNT  (TASK_BATCH_SIZE / sizeof(struct flt_task))

/* Create a new batch of task instances.  Link them all together via their next
 * fields */
static void
flt_task_batch_new(struct flt_priv *flt)
{
    struct flt_task  *task = malloc(TASK_BATCH_SIZE);
    struct flt_task  *curr;
    struct flt_task  *last = task + TASK_BATCH_COUNT - 1;

    /* The first task in the batch is reserved, and is used to keep track of the
     * batches that are owned by this context. */
    task->next = flt->batches;
    flt->batches = task;

    /* The remaining tasks in the batch can be used by the scheduler.  These
     * task instances start off local to this context, but might migrate to
     * other contexts while the scheduler runs.  That's perfectly fine; we keep
     * track of the batches separately so that we can free everything safely
     * when the fleet is freed. */
    for (curr = task + 1; curr < last; curr++) {
        curr->next = curr + 1;
    }
    last->next = flt->unused;
    flt->unused = task + 1;
}

static struct flt_task *
flt_reuse_task(struct flt *pflt, flt_task *func, void *ud,
               size_t min, size_t max);

static struct flt_task *
flt_create_task(struct flt *pflt, flt_task *func, void *ud,
                size_t min, size_t max)
{
    struct flt_priv  *flt = container_of(pflt, struct flt_priv, public);
    struct flt_task  *task;
    flt_task_batch_new(flt);
    flt->public.new_task = flt_reuse_task;
    task = flt->unused;
    flt->unused = task->next;
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    return task;
}

static struct flt_task *
flt_reuse_task(struct flt *pflt, flt_task *func, void *ud,
               size_t min, size_t max)
{
    struct flt_priv  *flt = container_of(pflt, struct flt_priv, public);
    struct flt_task  *task;
    task = flt->unused;
    flt->unused = task->next;
    if (unlikely(task->next == NULL)) {
        flt->public.new_task = flt_create_task;
    }
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    return task;
}

void
flt_task_free(struct flt_priv *flt, struct flt_task *task)
{
    task->next = flt->unused;
    flt->unused = task;
    flt->public.new_task = flt_reuse_task;
}


/*-----------------------------------------------------------------------
 * Execution contexts
 */

void
flt_init(struct flt_priv *flt, struct flt_fleet *fleet,
         size_t index, size_t count)
{
    flt->index = index;
    flt->count = count;
    flt->fleet = fleet;
    flt->ready = NULL;
    flt->later = NULL;
    flt->unused = NULL;
    flt->batches = NULL;
    flt->public.new_task = flt_create_task;
}

static void
flt_task_batch_list_done(struct flt_task *curr)
{
    struct flt_task  *next;
    while (curr != NULL) {
        next = curr->next;
        free(curr);
        curr = next;
    }
}

void
flt_done(struct flt_priv *flt)
{
    flt_task_batch_list_done(flt->batches);
}
