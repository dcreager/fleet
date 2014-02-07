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
flt_task(struct flt *flt, void *ud, size_t i);

struct flt {
    unsigned int  index;
    unsigned int  count;

    struct flt_task *
    (*new_task)(struct flt *, const char *, flt_task *, void *, size_t, size_t);
};

#define flt_task_new(flt, func, ud, i) \
    ((flt)->new_task((flt), #func, (func), (ud), (i), (i)+1))

#define flt_bulk_task_new(flt, func, ud, min, max) \
    ((flt)->new_task((flt), #func, (func), (ud), (min), (max)))


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
flt_fleet_set_context_count(struct flt_fleet *fleet,
                            unsigned int context_count);

void
flt_fleet_run_(struct flt_fleet *fleet, const char *name,
               flt_task *func, void *ud, size_t i);

#define flt_fleet_run(fleet, func, ud, i) \
    flt_fleet_run_(fleet, #func, func, ud, i)


/*-----------------------------------------------------------------------
 * Context-local data
 */

struct flt_local {
    void  *instances;
};

typedef void
flt_local_init_f(struct flt *flt, void *ud, void *instance);

typedef void
flt_local_done_f(struct flt *flt, void *ud, void *instance);

struct flt_local *
flt_local_new_size(struct flt *flt, size_t instance_size, void *ud,
                   flt_local_init_f *init_instance,
                   flt_local_done_f *done_instance);

#define flt_local_new(flt, type, ud, init, done) \
    flt_local_new_size((flt), sizeof(type), (ud), (init), (done))

void
flt_local_free(struct flt *flt, struct flt_local *local);

#define flt_local_get_index(flt, local, type, i) \
    ((type *) \
     ((char *) (local)->instances + \
      (i) * flt_round_to_cache_line(sizeof(type))))

#define flt_local_get(flt, local, type) \
    flt_local_get_index(flt, local, type, (flt)->index)

#define flt_local_foreach(flt, local, i, type, inst) \
    for ((i) = 0, (inst) = (local)->instances; (i) < (flt)->count; \
         (i)++, \
         (inst) = ((type *) (((char *) (inst)) + \
             flt_round_to_cache_line(sizeof(type)))))

#define flt_local_visit(flt, local, type, visit, ...) \
    do { \
        size_t  __i; \
        type  *__instance; \
        flt_local_foreach(flt, local, __i, type, __instance) { \
            (visit)((flt), __instance, __VA_ARGS__); \
        } \
    } while (0)


#endif /* FLEET_H */
