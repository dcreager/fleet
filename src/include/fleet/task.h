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


struct flt_priv;


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt_task {
    struct flt_task  *next;
    flt_task  *func;
    void  *ud;
    size_t  min;
    size_t  max;
};

FLT_INTERNAL
void
flt_task_free(struct flt_priv *flt, struct flt_task *task);

#define flt_task_run(f, t)  ((t)->func((f), (t)->ud, (t)->min))


/*-----------------------------------------------------------------------
 * Execution contexts
 */

struct flt_priv {
    struct flt  public;
    size_t  index;
    size_t  count;
    struct flt_fleet  *fleet;
    struct flt_task  *ready;
    struct flt_task  *later;
    struct flt_task  *unused;
    struct flt_task  *batches;
};

FLT_INTERNAL
void
flt_init(struct flt_priv *flt, struct flt_fleet *fleet,
         size_t index, size_t count);

FLT_INTERNAL
void
flt_done(struct flt_priv *flt);

#define flt_ready_queue_is_empty(flt)  ((flt)->ready == NULL)


/*-----------------------------------------------------------------------
 * Fleets
 */

struct flt_fleet {
    size_t  count;
    struct flt_priv  *contexts;
};


#endif /* FLEET_TASK_H */
