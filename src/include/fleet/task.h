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
#include "fleet/dllist.h"


struct flt_priv;


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt_task {
    struct flt_dllist_item  item;
    flt_task  *func;
    void  *ud;
    size_t  min;
    size_t  max;
};

FLT_INTERNAL
void
flt_task_free(struct flt_priv *flt, struct flt_task *task);

#define flt_task_run(f, t, i)  ((t)->func((f), (t)->ud, (i)))


/*-----------------------------------------------------------------------
 * Execution contexts
 */

struct flt_priv {
    struct flt  public;
    size_t  index;
    size_t  count;
    struct flt_fleet  *fleet;
    struct flt_dllist  ready;
    struct flt_dllist  unused;
    struct flt_dllist  batches;
};

FLT_INTERNAL
void
flt_init(struct flt_priv *flt, struct flt_fleet *fleet,
         size_t index, size_t count);

FLT_INTERNAL
void
flt_done(struct flt_priv *flt);


/*-----------------------------------------------------------------------
 * Fleets
 */

struct flt_fleet {
    size_t  count;
    struct flt_priv  *contexts;
};


#endif /* FLEET_TASK_H */
