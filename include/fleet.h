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
flt_task(struct flt *flt, void *u1, void *u2, void *u3, void *u4);

void
flt_run(struct flt *flt, flt_task *func,
        void *u1, void *u2, void *u3, void *u4);

void
flt_run_later(struct flt *flt, flt_task *func,
              void *u1, void *u2, void *u3, void *u4);

#define flt_return_to(flt, task, u1, u2, u3, u4) \
    ((task)((flt), (u1), (u2), (u3), (u4)))

void
flt_then(struct flt *flt,
         flt_task *first, void *fu1, void *fu2, void *fu3, void *fu4,
         flt_task *second, void *su1, void *su2, void *su3, void *su4);


struct flt_fleet *
flt_fleet_new(void);

void
flt_fleet_free(struct flt_fleet *fleet);

void
flt_fleet_run(struct flt_fleet *fleet, flt_task *func,
              void *u1, void *u2, void *u3, void *u4);


#endif /* FLEET_H */
