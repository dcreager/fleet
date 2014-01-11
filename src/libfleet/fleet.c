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

struct flt_task *
flt_task_new(struct flt *flt, flt_task *func,
             void *u1, void *u2, void *u3, void *u4,
             struct flt_task *after)
{
    struct flt_task  *task = malloc(sizeof(struct flt_task));
    task->func = func;
    task->u1 = u1;
    task->u2 = u2;
    task->u3 = u3;
    task->u4 = u4;
    task->after = after;
    return task;
}

void
flt_task_free(struct flt *flt, struct flt_task *task)
{
    free(task);
}


/*-----------------------------------------------------------------------
 * Execution contexts
 */

void
flt_init(struct flt *flt, struct flt_fleet *fleet, size_t index, size_t count)
{
    flt->index = index;
    flt->count = count;
    flt->fleet = fleet;
    flt->ready = NULL;
    flt->later = NULL;
}

static void
flt_task_list_done(struct flt_task *curr)
{
    struct flt_task  *next;
    while (curr != NULL) {
        next = curr->next;
        free(curr);
        curr = next;
    }
}

void
flt_done(struct flt *flt)
{
    flt_task_list_done(flt->ready);
    flt_task_list_done(flt->later);
}
