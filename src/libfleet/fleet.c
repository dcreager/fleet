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
#include "fleet/dllist.h"
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
static struct flt_task *
flt_task_batch_new(struct flt_priv *flt)
{
    size_t  i;
    struct flt_task  *task = malloc(TASK_BATCH_SIZE);
    struct flt_task  *first;
    struct flt_task  *curr;

    /* The first task in the batch is reserved, and is used to keep track of the
     * batches that are owned by this context. */
    flt_dllist_add_to_tail(&flt->batches, &task->item);

    /* This whole operation was kicked off because someone wants to create a new
     * task instance; the second instance is the batch is the one we'll use for
     * that. */
    first = task + 1;

    /* The remaining tasks in the batch can be used by the scheduler.  These
     * task instances start off local to this context, but might migrate to
     * other contexts while the scheduler runs.  That's perfectly fine; we keep
     * track of the batches separately so that we can free everything safely
     * when the fleet is freed. */
    for (i = 2, curr = first + 1; i < TASK_BATCH_COUNT; i++, curr++) {
        flt_dllist_add_to_tail(&flt->unused, &curr->item);
    }

    return first;
}

static struct flt_task *
flt_reuse_task(struct flt *pflt, flt_task *func, void *ud,
               size_t min, size_t max);

static struct flt_task *
flt_create_task(struct flt *pflt, flt_task *func, void *ud,
                size_t min, size_t max)
{
    struct flt_priv  *flt = container_of(pflt, struct flt_priv, public);
    struct flt_task  *task = flt_task_batch_new(flt);
    flt->public.new_task = flt_reuse_task;
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
    struct flt_dllist_item  *head = flt_dllist_start(&flt->unused);
    struct flt_task  *task = container_of(head, struct flt_task, item);
    if (unlikely(flt_dllist_is_end(&flt->unused, head->next))) {
        flt->public.new_task = flt_create_task;
    }
    flt_dllist_remove(head);
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    return task;
}

void
flt_task_free(struct flt_priv *flt, struct flt_task *task)
{
    flt_dllist_add_to_head(&flt->unused, &task->item);
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
    flt_dllist_init(&flt->ready);
    flt_dllist_init(&flt->unused);
    flt_dllist_init(&flt->batches);
    flt->public.new_task = flt_create_task;
}

static void
flt_task_batch_list_done(struct flt_dllist *list)
{
    struct flt_dllist_item  *curr;
    struct flt_dllist_item  *next;
    for (curr = flt_dllist_start(list);
         !flt_dllist_is_end(list, curr); curr = next) {
        struct flt_task  *task = container_of(curr, struct flt_task, item);
        next = curr->next;
        free(task);
    }
}

void
flt_done(struct flt_priv *flt)
{
    flt_task_batch_list_done(&flt->batches);
}
