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

#include "fleet/threads.h"


struct flt_priv;


/*-----------------------------------------------------------------------
 * Tasks
 */

/* Task is detached if `group == NULL`.  Task is pending if `group->state ==
 * FLT_TASK_GROUP_STOPPED`.  Task is ready if `group->state ==
 * FLT_TASK_GROUP_STARTED`.
 *
 * If task is detached, then it will not be in any linked list.  If it's
 * pending, it will be in in `tasks` list of one of the instances in
 * `group->ctxs`.  If it's ready, it will be in the `ready` list of one of the
 * fleet's execution contexts. */

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

/* Each task group maintains some per-context state so that we can do certain
 * operations without needing any thread synchronization.
 *
 * An execution context is "active" for a particular task group if the group has
 * been started, and there are any ready tasks in the group that are currently
 * assigned to that context (ie, the tasks are in the `ready` queue of that
 * context). */

struct flt_task_group_ctx {
    /* The group that this per-context object belongs to */
    struct flt_task_group  *group;
    /* A linked list of groups that will be started after this group finishes */
    struct flt_task_group  *after;
    /* A list of pending tasks that belong to this group.  (This will be empty
     * once the group is started.) */
    struct cork_dllist  tasks;
    /* The number of pending or ready tasks in this group assigned to this
     * execution context. */
    size_t  task_count;
    /* The number of executions in any pending or ready tasks in this group
     * assigned to this execution context.  (This includes all of the executions
     * in each bulkt ask, whereas task_count only counts a bulk task once.) */
    size_t  execution_count;
};

#define FLT_TASK_GROUP_STOPPED  0
#define FLT_TASK_GROUP_STARTED  1

struct flt_task_group {
    struct cork_dllist_item  item;
    struct flt_local  *ctxs;
    struct flt_counter  active_ctx_count;
    struct flt_task_group  *next_after;
    unsigned int  state;
};

CORK_LOCAL
void
flt_task_group_free(struct flt_priv *flt, struct flt_task_group *group);

/* Once an execution context finishes executing a task, it should use this
 * function to decrement its group's `task_count` field. */
CORK_LOCAL
void
flt_task_group_decrement(struct flt_priv *flt, struct flt_task_group *group);


/*-----------------------------------------------------------------------
 * Execution contexts
 */

struct flt_priv {
    struct flt_spinlock  lock;
    struct flt  public;
    struct flt_fleet  *fleet;
    struct cork_dllist  ready;
    struct cork_dllist  unused;
    struct cork_dllist  batches;
    struct cork_dllist  groups;
    size_t  execution_count;
};

CORK_LOCAL
void
flt_init(struct flt_priv *flt, struct flt_fleet *fleet,
         size_t index, size_t count);

CORK_LOCAL
void
flt_done(struct flt_priv *flt);

CORK_LOCAL
struct flt_task *
flt_current_task(struct flt_priv *flt);

CORK_LOCAL
struct flt_task_group *
flt_current_group(struct flt_priv *flt);


/*-----------------------------------------------------------------------
 * Fleets
 */

struct flt_fleet {
    struct flt_priv  *contexts;
    unsigned int  count;
    struct flt_counter  active_count;
};


#endif /* FLEET_TASK_H */
