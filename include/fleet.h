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
flt_migrate_f(struct flt *from_ctx, struct flt *to_ctx, void *ud, size_t i);


void
flt_run(struct flt *flt, flt_task_f *func, void *ud, size_t i);

void
flt_run_migratable(struct flt *flt, flt_task_f *func, void *ud, size_t i,
                   flt_migrate_f *migrate);

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

struct flt_local;

typedef void
flt_local_init_f(struct flt *flt, void *ud, void *instance, unsigned int index);

typedef void
flt_local_done_f(struct flt *flt, void *ud, void *instance, unsigned int index);


struct flt_local *
flt_local_new_size(struct flt *flt, size_t instance_size, void *ud,
                   flt_local_init_f *init_instance,
                   flt_local_done_f *done_instance);

#define flt_local_new(flt, type, ud, init, done) \
    flt_local_new_size((flt), sizeof(type), (ud), (init), (done))

void
flt_local_free(struct flt *flt, struct flt_local *local);

void
flt_local_ctx_free(struct flt *flt, void *instance);


void *
flt_local_get(struct flt *flt, struct flt_local *local);

void *
flt_local_get_index(struct flt *flt, struct flt_local *local,
                    unsigned int index);

void *
flt_local_ctx_get_index(struct flt *flt, void *instance, unsigned int index);

void *
flt_local_ctx_migrate(struct flt *from, struct flt *to, void *from_instance);


#define flt_local_foreach(flt, type, local, i, inst) \
    for ((inst) = flt_local_get_index((flt), (local), (i) = 0); \
         (i) < (flt)->count; \
         (inst) = flt_local_get_index((flt), (local), ++(i))) \

#define flt_local_visit(flt, type, local, visit, ...) \
    do { \
        unsigned int  __i; \
        type  *__instance; \
        flt_local_foreach(flt, type, local, __i, __instance) { \
            (visit)((flt), __i, __instance, __VA_ARGS__); \
        } \
    } while (0)


#define flt_local_ctx_foreach(flt, type, this_inst, i, inst) \
    for ((inst) = flt_local_ctx_get_index((flt), (this_inst), (i) = 0); \
         (i) < (flt)->count; \
         (inst) = flt_local_ctx_get_index((flt), (this_inst), ++(i))) \

#define flt_local_ctx_visit(flt, type, this_inst, visit, ...) \
    do { \
        unsigned int  __i; \
        type  *__instance; \
        flt_local_ctx_foreach(flt, type, this_inst, __i, __instance) { \
            (visit)((flt), __i, __instance, __VA_ARGS__); \
        } \
    } while (0)


/*-----------------------------------------------------------------------
 * Semaphore counters
 */

struct flt_scounter;

struct flt_scounter *
flt_scounter_new(struct flt *flt);

struct flt_scounter_ctx *
flt_scounter_get(struct flt *flt, struct flt_scounter *counter,
                 unsigned int index);

void
flt_scounter_inc(struct flt *flt, struct flt_scounter *counter);

void
flt_scounter_ctx_inc(struct flt *flt, struct flt_scounter_ctx *counter_ctx);

/* Returns true if this decrements the counter to 0. */
bool
flt_scounter_ctx_dec(struct flt *flt, struct flt_scounter_ctx *counter_ctx);

struct flt_scounter_ctx *
flt_scounter_ctx_migrate(struct flt *from, struct flt *to,
                         struct flt_scounter_ctx *counter_ctx);


#endif /* FLEET_H */
