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
 * Fleets
 */

struct flt_fleet;

struct flt_fleet *
flt_fleet_new(void);

void
flt_fleet_free(struct flt_fleet *fleet);

void
flt_fleet_run(struct flt_fleet *fleet, flt_task *func, void *ud, size_t i);


#endif /* FLEET_H */
