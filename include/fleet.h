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
flt_current_group(struct flt *flt);

void
flt_run_after_group(struct flt *flt, struct flt_task_group *group,
                    struct flt_task *task);

void
flt_run_after_current_group(struct flt *flt, struct flt_task *task);


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
flt_local_new_f(void *ud);

typedef void
flt_local_free_f(void *ud, void *instance);

struct flt_local *
flt_local_new(struct flt *flt, void *ud,
              flt_local_new_f *new_instance,
              flt_local_free_f *free_instance);

void
flt_local_free(struct flt_local *local);

void *
flt_local_get(struct flt *flt, struct flt_local *local);

#define flt_local_get(flt, local) \
    ((local)->instances[(flt)->index])

typedef void
flt_local_visit_f(struct flt *flt, void *ud, void *instance);

#define flt_local_for_each(flt, local, ud, visit) \
    do { \
        size_t  __i; \
        for (__i = 0; __i < (flt)->count; __i++) { \
            (visit)((flt), (ud), (local)->instances[__i]); \
        } \
    } while (0)


#endif /* FLEET_H */
