/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef FLEET_H
#define FLEET_H

#include <stddef.h>


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt;
struct flt_task;

typedef void
flt_task(struct flt *flt, void *ud, size_t i);

struct flt {
    size_t  index;
    size_t  count;

    struct flt_task *
    (*new_task)(struct flt *, flt_task *, void *, size_t, size_t);
};

#define flt_task_new(flt, func, ud, i) \
    ((flt)->new_task((flt), (func), (ud), (i), (i)+1))

#define flt_bulk_task_new(flt, func, ud, min, max) \
    ((flt)->new_task((flt), (func), (ud), (min), (max)))


void
flt_run(struct flt *flt, struct flt_task *task);

void
flt_run_later(struct flt *flt, struct flt_task *task);

#define flt_return_to(flt, task, ud, i)  ((task)((flt), (ud), (i)))


/*-----------------------------------------------------------------------
 * Task groups
 */

struct flt_task_group;

struct flt_task_group *
flt_task_group_new(struct flt *flt);

/* Cannot be called after the group is running.  Thread-safe for `group` */
void
flt_task_group_add(struct flt *flt, struct flt_task_group *group,
                   struct flt_task *task);

void
flt_task_group_start(struct flt *flt, struct flt_task_group *group);

/* Thread-safe for `group`, but not for `after` */
void
flt_task_group_run_after(struct flt *flt, struct flt_task_group *group,
                         struct flt_task_group *after);

/* Not thread-safe for `after` */
void
flt_task_group_run_after_current(struct flt *flt, struct flt_task_group *after);


/*-----------------------------------------------------------------------
 * Fleets
 */

struct flt_fleet;

struct flt_fleet *
flt_fleet_new(void);

void
flt_fleet_free(struct flt_fleet *fleet);

void
flt_fleet_run(struct flt_fleet *fleet, flt_task *func, void *ud, size_t i);


/*-----------------------------------------------------------------------
 * Context-local data
 */

struct flt_local {
    void  **instances;
};

typedef void *
flt_local_new_f(struct flt *flt, void *ud);

typedef void
flt_local_free_f(struct flt *flt, void *ud, void *instance);

struct flt_local *
flt_local_new(struct flt *flt, void *ud,
              flt_local_new_f *new_instance,
              flt_local_free_f *free_instance);

void
flt_local_free(struct flt *flt, struct flt_local *local);

void *
flt_local_get(struct flt *flt, struct flt_local *local);

#define flt_local_get(flt, local) \
    ((local)->instances[(flt)->index])

typedef void
flt_local_visit_f(struct flt *flt, void *ud, void *instance);

#define flt_local_foreach(flt, local, i, inst) \
    for ((i) = 0, (inst) = (local)->instances[0]; (i) < (flt)->count; \
         (inst) = (local)->instances[++(i)])

#define flt_local_visit(flt, local, ud, visit) \
    do { \
        size_t  __i; \
        void  *__instance; \
        flt_local_foreach(flt, local, __i, __instance) { \
            (visit)((flt), (ud), __instance); \
        } \
    } while (0)


#endif /* FLEET_H */
