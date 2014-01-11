/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef FLEET_TASK_H
#define FLEET_TASK_H

#include "fleet/internal.h"


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt_task {
    struct flt_task  *next;
    flt_task  *func;
    void  *u1;
    void  *u2;
    void  *u3;
    void  *u4;
    struct flt_task  *after;
};

FLT_INTERNAL
struct flt_task *
flt_task_new(struct flt *flt, flt_task *func,
             void *u1, void *u2, void *u3, void *u4,
             struct flt_task *after);

FLT_INTERNAL
void
flt_task_free(struct flt *flt, struct flt_task *task);

#define flt_task_run(f, t) \
    ((t)->func((f), (t)->u1, (t)->u2, (t)->u3, (t)->u4))


/*-----------------------------------------------------------------------
 * Execution contexts
 */

struct flt {
    size_t  index;
    size_t  count;
    struct flt_fleet  *fleet;
    struct flt_task  *ready;
    struct flt_task  *later;
};

FLT_INTERNAL
void
flt_init(struct flt *flt, struct flt_fleet *fleet, size_t index, size_t count);

FLT_INTERNAL
void
flt_done(struct flt *flt);

#define flt_ready_queue_is_empty(flt)  ((flt)->ready == NULL)


/*-----------------------------------------------------------------------
 * Fleets
 */

struct flt_fleet {
    size_t  count;
    struct flt  *contexts;
};


#endif /* FLEET_TASK_H */
