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


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt;
struct flt_fleet;

typedef void
flt_task(struct flt *flt, void *ud);


void
flt_run(struct flt *flt, flt_task *func, void *ud);

void
flt_run_later(struct flt *flt, flt_task *func, void *ud);

#define flt_return_to(flt, task, ud)  ((task)((flt), (ud)))

void
flt_then(struct flt *flt, flt_task *first, void *fud,
         flt_task *second, void *sud);


struct flt_fleet *
flt_fleet_new(void);

void
flt_fleet_free(struct flt_fleet *fleet);

void
flt_fleet_run(struct flt_fleet *fleet, flt_task *func, void *ud);


#endif /* FLEET_H */
