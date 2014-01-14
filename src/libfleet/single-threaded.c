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
flt_run(struct flt *flt, flt_task *func, void *ud)
{
    struct flt_task  *task = flt_task_new(flt, func, ud, NULL);
    task->next = flt->ready;
    flt->ready = task;
}

void
flt_run_later(struct flt *flt, flt_task *func, void *ud)
{
    struct flt_task  *task = flt_task_new(flt, func, ud, NULL);
    task->next = flt->later;
    flt->later = task;
}

void
flt_then(struct flt *flt, flt_task *ffunc, void *fud,
         flt_task *sfunc, void *sud)
{
    struct flt_task  *second = flt_task_new(flt, sfunc, sud, NULL);
    struct flt_task  *first = flt_task_new(flt, ffunc, fud, second);
    first->next = flt->ready;
    flt->ready = first;
}


static void
flt_loop(struct flt *flt)
{
    do {
        while (!flt_ready_queue_is_empty(flt)) {
            struct flt_task  *task = flt->ready;
            flt->ready = task->next;
            do {
                struct flt_task  *after = task->after;
                flt_task_run(flt, task);
                flt_task_free(flt, task);
                task = after;
            } while (task != NULL);
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
    fleet->contexts = malloc(sizeof(struct flt));
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
flt_fleet_run(struct flt_fleet *fleet, flt_task *func, void *ud)
{
    struct flt  *flt = fleet->contexts;
    struct flt_task  *task = flt_task_new(flt, func, ud, NULL);
    task->next = flt->ready;
    flt->ready = task;
    flt_loop(flt);
}
