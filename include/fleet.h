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
struct flt_fleet;

typedef void
flt_task(struct flt *flt, void *ud, size_t i);


void
flt_run(struct flt *flt, flt_task *func, void *ud, size_t i);

void
flt_run_later(struct flt *flt, flt_task *func, void *ud, size_t i);

#define flt_return_to(flt, task, ud, i)  ((task)((flt), (ud), (i)))

void
flt_then(struct flt *flt, flt_task *first, void *fud, size_t fi,
         flt_task *second, void *sud, size_t si);


void
flt_run_many(struct flt *flt, flt_task *func, void *ud, size_t min, size_t max);

void
flt_run_many_later(struct flt *flt, flt_task *func, void *ud,
                   size_t min, size_t max);


struct flt_fleet *
flt_fleet_new(void);

void
flt_fleet_free(struct flt_fleet *fleet);

void
flt_fleet_run(struct flt_fleet *fleet, flt_task *func, void *ud, size_t i);


#endif /* FLEET_H */
