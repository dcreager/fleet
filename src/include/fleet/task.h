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

#include "libcork/core.h"
#include "libcork/ds.h"


struct flt_priv;


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt_task {
    struct cork_dllist_item  item;
    struct flt_task_group  *group;
    flt_task  *func;
    void  *ud;
    size_t  min;
    size_t  max;
};

CORK_LOCAL
void
flt_task_free(struct flt_priv *flt, struct flt_task *task);

#define flt_task_run(f, t, i)  ((t)->func((f), (t)->ud, (i)))


/*-----------------------------------------------------------------------
 * Task groups
 */

struct flt_task_group {
    struct cork_dllist_item  item;
    struct cork_dllist  after;
    size_t  active_tasks;
    size_t  after_tasks;
};

CORK_LOCAL
struct flt_task_group *
flt_task_group_new(struct flt_priv *flt);

CORK_LOCAL
void
flt_task_group_free(struct flt_priv *flt, struct flt_task_group *group);

CORK_LOCAL
void
flt_task_group_decrement(struct flt_priv *flt, struct flt_task_group *group);


/*-----------------------------------------------------------------------
 * Execution contexts
 */

struct flt_priv {
    struct flt  public;
    struct flt_fleet  *fleet;
    struct cork_dllist  ready;
    struct cork_dllist  unused;
    struct cork_dllist  batches;
    struct cork_dllist  groups;
};

CORK_LOCAL
void
flt_init(struct flt_priv *flt, struct flt_fleet *fleet,
         size_t index, size_t count);

CORK_LOCAL
void
flt_done(struct flt_priv *flt);

static inline
struct flt_task_group *
flt_current_group_priv(struct flt_priv *flt)
{
    struct cork_dllist_item  *head = cork_dllist_start(&flt->groups);
    return cork_container_of(head, struct flt_task_group, item);
}


/*-----------------------------------------------------------------------
 * Fleets
 */

struct flt_fleet {
    size_t  count;
    struct flt_priv  *contexts;
};


#endif /* FLEET_TASK_H */
