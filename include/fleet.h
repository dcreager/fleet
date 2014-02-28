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

#include <stdbool.h>
#include <stddef.h>


#define FLT_CACHE_LINE_SIZE  64

#define flt_round_to_cache_line(sz) \
    (((sz) % FLT_CACHE_LINE_SIZE) == 0? (sz): \
     (((sz) / FLT_CACHE_LINE_SIZE) + 1) * FLT_CACHE_LINE_SIZE)


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt;
struct flt_task;

typedef void
flt_task_f(struct flt *flt, void *ud, size_t i);

typedef void *
flt_migrate_f(struct flt *from_ctx, struct flt *to_ctx, void *ud, size_t i,
              void *migrate_ud);


void
flt_run(struct flt *flt, flt_task_f *func, void *ud, size_t i);

void
flt_run_migratable(struct flt *flt, flt_task_f *func, void *ud, size_t i,
                   flt_migrate_f *migrate, void *migrate_ud);

#define flt_return_to(flt, task, ud, i)  ((task)((flt), (ud), (i)))


/*-----------------------------------------------------------------------
 * Fleets
 */

struct flt {
    unsigned int  index;
    unsigned int  count;
};

struct flt_fleet;

struct flt_fleet *
flt_fleet_new(void);

void
flt_fleet_free(struct flt_fleet *fleet);

void
flt_fleet_set_context_count(struct flt_fleet *fleet,
                            unsigned int context_count);

void
flt_fleet_run(struct flt_fleet *fleet, flt_task_f *func, void *ud, size_t i);


/*-----------------------------------------------------------------------
 * Context-local data
 */

typedef void
flt_local_init_f(struct flt *flt, void *ud, void *instance, unsigned int index);

typedef void
flt_local_done_f(struct flt *flt, void *ud, void *instance, unsigned int index);


void *
flt_local_new_size(struct flt *flt, size_t instance_size, void *ud,
                   flt_local_init_f *init_instance,
                   flt_local_done_f *done_instance);

#define flt_local_new(flt, type, ud, init, done) \
    flt_local_new_size((flt), sizeof(type), (ud), (init), (done))

void
flt_local_free(struct flt *flt, void *instance);


void *
flt_local_get(struct flt *flt, void *instance, unsigned int index);

void *
flt_local_migrate(struct flt *from, struct flt *to, void *from_instance);

#define flt_local_foreach(flt, type, local, i, inst) \
    for ((inst) = flt_local_get((flt), (local), (i) = 0); \
         (i) < (flt)->count; \
         (inst) = flt_local_get((flt), (local), ++(i))) \

#define flt_local_visit(flt, type, local, visit, ...) \
    do { \
        unsigned int  __i; \
        type  *__instance; \
        flt_local_foreach(flt, type, local, __i, __instance) { \
            (visit)((flt), __i, __instance, __VA_ARGS__); \
        } \
    } while (0)


/*-----------------------------------------------------------------------
 * Semaphore counters
 */

struct flt_scounter;

struct flt_scounter *
flt_scounter_new(struct flt *flt);

void
flt_scounter_free(struct flt *flt, struct flt_scounter *counter);

struct flt_scounter *
flt_scounter_get(struct flt *flt, struct flt_scounter *counter,
                 unsigned int index);

void
flt_scounter_inc(struct flt *flt, struct flt_scounter *counter);

/* Returns true if this decrements the counter to 0. */
bool
flt_scounter_dec(struct flt *flt, struct flt_scounter *counter);

struct flt_scounter *
flt_scounter_migrate(struct flt *from, struct flt *to,
                     struct flt_scounter *counter);


#endif /* FLEET_H */
