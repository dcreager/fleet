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
 * Single-threaded fleet scheduler
 */

void
flt_run(struct flt *pflt, struct flt_task *task)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    task->group->active_tasks++;
    cork_dllist_add_to_head(&flt->ready, &task->item);
}

void
flt_run_later(struct flt *pflt, struct flt_task *task)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    task->group->active_tasks++;
    cork_dllist_add_to_tail(&flt->ready, &task->item);
}


#define FLT_ROUND_SIZE  32

static size_t
flt_pop_and_run_one(struct flt_priv *flt, size_t max_count)
{
    struct cork_dllist_item  *head = cork_dllist_start(&flt->ready);
    struct flt_task  *task = cork_container_of(head, struct flt_task, item);
    size_t  max = task->min + max_count;

    if (max < task->max) {
        /* There are more iterations in this bulk task than we can execute
         * during this lock acquisition.  So only execute the first max_count
         * iterations. */
        size_t  i;
        for (i = task->min; i < max; i++) {
            flt_task_run(&flt->public, task, i);
        }
        task->min = max;
        return max_count;
    } else {
        /* We can execute all of the iterations in this bulk task without
         * exceeding our allotment for this lock acquisition.  So execute them
         * all and retire the task. */
        size_t  i;
        size_t  count;
        max = task->max;
        for (i = task->min; i < max; i++) {
            flt_task_run(&flt->public, task, i);
        }
        count = max - task->min;
        cork_dllist_remove(head);
        flt_task_group_decrement(flt, task->group);
        flt_task_free(flt, task);
        return count;
    }
}

static void
flt_loop(struct flt_priv *flt)
{
    while (true) {
        size_t  max_count = FLT_ROUND_SIZE;
        flt_spinlock_lock(&flt->lock);
        while (max_count > 0) {
            if (cork_dllist_is_empty(&flt->ready)) {
                flt_spinlock_unlock(&flt->lock);
                return;
            } else {
                size_t  executed_count = flt_pop_and_run_one(flt, max_count);
                max_count -= executed_count;
            }
        }
        flt_spinlock_unlock(&flt->lock);
    }
}


struct flt_fleet *
flt_fleet_new(void)
{
    struct flt_fleet  *fleet = cork_new(struct flt_fleet);
    fleet->count = 1;
    fleet->contexts = cork_new(struct flt_priv);
    flt_init(fleet->contexts, fleet, 0, 1);
    return fleet;
}

void
flt_fleet_free(struct flt_fleet *fleet)
{
    flt_done(fleet->contexts);
    free(fleet->contexts);
    free(fleet);
}

void
flt_fleet_run(struct flt_fleet *fleet, flt_task *func, void *ud, size_t i)
{
    struct flt_priv  *flt = fleet->contexts;
    struct flt_task  *task = flt_task_new(&flt->public, func, ud, i);
    flt_run(&flt->public, task);
    flt_loop(flt);
}
