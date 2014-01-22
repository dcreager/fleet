/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include "libcork/core.h"
#include "libcork/ds.h"

#include "fleet.h"
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
    struct flt_task  *task = cork_malloc(TASK_BATCH_SIZE);
    struct flt_task  *first;
    struct flt_task  *curr;

    /* The first task in the batch is reserved, and is used to keep track of the
     * batches that are owned by this context. */
    cork_dllist_add_to_tail(&flt->batches, &task->item);

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
        cork_dllist_add_to_tail(&flt->unused, &curr->item);
    }

    return first;
}

static void
flt_task_batch_free(struct flt_priv *flt, struct flt_task *batch)
{
    free(batch);
}

static struct flt_task *
flt_reuse_task(struct flt *pflt, flt_task *func, void *ud,
               size_t min, size_t max);

static struct flt_task *
flt_create_task(struct flt *pflt, flt_task *func, void *ud,
                size_t min, size_t max)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task  *task = flt_task_batch_new(flt);
    flt->public.new_task = flt_reuse_task;
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    task->group = flt_current_group_priv(flt);
    return task;
}

static struct flt_task *
flt_reuse_task(struct flt *pflt, flt_task *func, void *ud,
               size_t min, size_t max)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct cork_dllist_item  *head = cork_dllist_start(&flt->unused);
    struct flt_task  *task = cork_container_of(head, struct flt_task, item);
    if (CORK_UNLIKELY(cork_dllist_is_end(&flt->unused, head->next))) {
        flt->public.new_task = flt_create_task;
    }
    cork_dllist_remove(head);
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    task->group = flt_current_group_priv(flt);
    return task;
}

void
flt_task_free(struct flt_priv *flt, struct flt_task *task)
{
    cork_dllist_add_to_head(&flt->unused, &task->item);
    flt->public.new_task = flt_reuse_task;
}


/*-----------------------------------------------------------------------
 * Task groups
 */

struct flt_task_group *
flt_task_group_new(struct flt_priv *flt)
{
    struct flt_task_group  *group = cork_new(struct flt_task_group);
    group->active_tasks = 0;
    group->after_tasks = 0;
    cork_dllist_init(&group->after);
    return group;
}

void
flt_task_group_free(struct flt_priv *pflt, struct flt_task_group *group)
{
    free(group);
}

void
flt_task_group_decrement(struct flt_priv *flt, struct flt_task_group *group)
{
    if (--group->active_tasks == 0) {
        if (!cork_dllist_is_empty(&group->after)) {
            cork_dllist_add_list_to_head(&flt->ready, &group->after);
        }
    }
}

void
flt_run_after_group(struct flt *flt, struct flt_task_group *group,
                    struct flt_task *task)
{
    cork_dllist_add_to_tail(&group->after, &task->item);
}

void
flt_run_after_current_group(struct flt *pflt, struct flt_task *task)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group  *group = flt_current_group_priv(flt);
    cork_dllist_add_to_tail(&group->after, &task->item);
}


/*-----------------------------------------------------------------------
 * Execution contexts
 */

void
flt_init(struct flt_priv *flt, struct flt_fleet *fleet,
         size_t index, size_t count)
{
    struct flt_task_group  *group;
    flt->public.index = index;
    flt->public.count = count;
    flt->fleet = fleet;
    flt->public.new_task = flt_create_task;
    cork_dllist_init(&flt->ready);
    cork_dllist_init(&flt->unused);
    cork_dllist_init(&flt->batches);
    cork_dllist_init(&flt->groups);
    group = flt_task_group_new(flt);
    cork_dllist_add_to_head(&flt->groups, &group->item);
}

static void
flt_task_batch_list_done(struct flt_priv *flt, struct cork_dllist *list)
{
    struct cork_dllist_item  *curr;
    struct cork_dllist_item  *next;
    struct flt_task  *task;
    cork_dllist_foreach(list, curr, next, struct flt_task, task, item) {
        flt_task_batch_free(flt, task);
    }
}

static void
flt_task_group_list_done(struct flt_priv *flt, struct cork_dllist *list)
{
    struct cork_dllist_item  *curr;
    struct cork_dllist_item  *next;
    struct flt_task_group  *group;
    cork_dllist_foreach(list, curr, next, struct flt_task_group, group, item) {
        flt_task_group_free(flt, group);
    }
}

void
flt_done(struct flt_priv *flt)
{
    flt_task_batch_list_done(flt, &flt->batches);
    flt_task_group_list_done(flt, &flt->groups);
}

struct flt_task_group *
flt_current_group(struct flt *pflt)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    return flt_current_group_priv(flt);
}
