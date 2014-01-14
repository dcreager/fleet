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
#include "fleet/task.h"


/*-----------------------------------------------------------------------
 * Single-threaded fleet scheduler
 */

void
flt_run(struct flt *pflt, struct flt_task *task)
{
    struct flt_priv  *flt = container_of(pflt, struct flt_priv, public);
    task->next = flt->ready;
    flt->ready = task;
}

void
flt_run_later(struct flt *pflt, struct flt_task *task)
{
    struct flt_priv  *flt = container_of(pflt, struct flt_priv, public);
    task->next = flt->later;
    flt->later = task;
}


static void
flt_task_run_all(struct flt_priv *flt, struct flt_task *task)
{
    while (task->min < task->max) {
        flt_task_run(&flt->public, task);
        task->min++;
    }
}


static void
flt_loop(struct flt_priv *flt)
{
    do {
        while (!flt_ready_queue_is_empty(flt)) {
            struct flt_task  *task = flt->ready;
            flt->ready = task->next;
            flt_task_run_all(flt, task);
            flt_task_free(flt, task);
        }
        flt->ready = flt->later;
        flt->later = NULL;
    } while (!flt_ready_queue_is_empty(flt));
}


struct flt_fleet *
flt_fleet_new(void)
{
    struct flt_fleet  *fleet = malloc(sizeof(struct flt_fleet));
    fleet->count = 1;
    fleet->contexts = malloc(sizeof(struct flt_priv));
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
