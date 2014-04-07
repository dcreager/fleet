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
#include <stdlib.h>


#define FLT_CACHE_LINE_SIZE  64

#define flt_round_to_cache_line(sz) \
    (((sz) % FLT_CACHE_LINE_SIZE) == 0? (sz): \
     (((sz) / FLT_CACHE_LINE_SIZE) + 1) * FLT_CACHE_LINE_SIZE)


/*-----------------------------------------------------------------------
 * Compiler shenanigans
 */

/* Assume that __builtin_expect is available in any extant version of GCC.
 * Update this test if we find out that's not the case. */
#if defined(__GNUC__)

#if !defined(FLT_HAVE_EXPECT)
#define FLT_HAVE_EXPECT  1
#endif

#if !defined(FLT_HAVE_INLINE)
#define FLT_HAVE_INLINE  1
#endif

#if !defined(FLT_HAVE_NOINLINE)
#define FLT_HAVE_NOINLINE  1
#endif

#if !defined(FLT_HAVE_UNUSED)
#define FLT_HAVE_UNUSED  1
#endif

#endif /* defined(__GCC__) */

#if FLT_HAVE_EXPECT
#define FLT_LIKELY(expr)    (__builtin_expect((expr), 1))
#define FLT_UNLIKELY(expr)  (__builtin_expect((expr), 0))
#else
#define FLT_LIKELY(expr)    (expr)
#define FLT_UNLIKELY(expr)  (expr)
#endif

#if FLT_HAVE_INLINE
#define FLT_INLINE  inline
#else
#define FLT_INLINE
#endif

#if FLT_HAVE_NOINLINE
#define FLT_NOINLINE  __attribute__((noinline))
#else
#define FLT_NOINLINE
#endif

#if FLT_HAVE_UNUSED
#define FLT_UNUSED  __attribute__((unused))
#else
#define FLT_UNUSED
#endif


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt;
struct flt_task;

typedef void
flt_task_run_f(struct flt *flt, struct flt_task *task);


struct flt_task {
    flt_task_run_f  *run;
    void  *ud;
};

struct flt_task *
flt_task_new_unscheduled(struct flt *flt, flt_task_run_f *run, void *ud);

struct flt_task *
flt_task_new_scheduled(struct flt *flt, flt_task_run_f *run, void *ud);

void
flt_task_free(struct flt *flt, struct flt_task *task);

void
flt_task_schedule(struct flt *flt, struct flt_task *task);

void
flt_task_reschedule(struct flt *flt, struct flt_task *task);

FLT_UNUSED
static FLT_INLINE void
flt_return_to(struct flt *flt, flt_task_run_f *run, void *ud)
{
    struct flt_task  task = { run, ud };
    return run(flt, &task);
}


typedef void
flt_task_migrate_f(struct flt *from_ctx, struct flt *to_ctx,
                   struct flt_task *task, void *ud);

void
flt_task_add_on_migrate(struct flt *flt, struct flt_task *task,
                        flt_task_migrate_f *migrate, void *ud);


typedef void
flt_task_finished_f(struct flt *flt, struct flt_task *task, void *ud);

void
flt_task_add_on_finished(struct flt *flt, struct flt_task *task,
                         flt_task_finished_f *finished, void *ud);


/*-----------------------------------------------------------------------
 * Fleets
 */

struct flt {
    unsigned int  index;
    unsigned int  count;
    /* Memory allocators */
    void  *unused8;
    void  *unused64;
    void  *unused256;
    void  *unused1024;
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
flt_fleet_run(struct flt_fleet *fleet, flt_task_run_f *run, void *ud);


/*-----------------------------------------------------------------------
 * Memory allocation
 */

#define flt_define_allocator_functions(sz) \
\
FLT_UNUSED \
static FLT_INLINE void * \
flt_claim_##sz(struct flt *flt) \
{ \
    void  *result = flt->unused##sz; \
    void  **next = result; \
    flt->unused##sz = *next; \
    return result; \
} \
\
FLT_UNUSED \
static FLT_INLINE void \
flt_cancel_claim_##sz(struct flt *flt, void *ptr) \
{ \
    void  **next = ptr; \
    *next = flt->unused##sz; \
    flt->unused##sz = ptr; \
} \
\
void * \
flt_alloc_##sz(struct flt *flt, void *ptr); \
\
FLT_UNUSED \
static FLT_INLINE void * \
flt_finish_claim_##sz(struct flt *flt, void *ptr) \
{ \
    if (FLT_UNLIKELY(flt->unused##sz == NULL)) { \
        return flt_alloc_##sz(flt, ptr); \
    } else { \
        return ptr; \
    } \
} \
\
FLT_UNUSED \
static FLT_INLINE void \
flt_release_##sz(struct flt *flt, void *ptr) \
{ \
    void  **next = ptr; \
    *next = flt->unused##sz; \
    flt->unused##sz = ptr; \
}

flt_define_allocator_functions(8);
flt_define_allocator_functions(64);
flt_define_allocator_functions(256);
flt_define_allocator_functions(1024);

#undef flt_define_allocator_functions

void *
flt_alloc_any(struct flt *flt, size_t size);

void
flt_dealloc_any(struct flt *flt, size_t size, void *ptr);


#define flt_claim(flt, type) \
    ((sizeof(type) <= 8)? flt_claim_8((flt)): \
     (sizeof(type) <= 64)? flt_claim_64((flt)): \
     (sizeof(type) <= 256)? flt_claim_256((flt)): \
     (sizeof(type) <= 1024)? flt_claim_1024((flt)): \
     flt_alloc_any((flt), sizeof(type)))

#define flt_cancel_claim(flt, type, ptr) \
    ((sizeof(type) <= 8)? flt_cancel_claim_8((flt), (ptr)): \
     (sizeof(type) <= 64)? flt_cancel_claim_64((flt), (ptr)): \
     (sizeof(type) <= 256)? flt_cancel_claim_256((flt), (ptr)): \
     (sizeof(type) <= 1024)? flt_cancel_claim_1024((flt), (ptr)): \
     flt_dealloc_any((flt), sizeof(type), (ptr)))

#define flt_finish_claim(flt, type, ptr) \
    ((sizeof(type) <= 8)? flt_finish_claim_8((flt), (ptr)): \
     (sizeof(type) <= 64)? flt_finish_claim_64((flt), (ptr)): \
     (sizeof(type) <= 256)? flt_finish_claim_256((flt), (ptr)): \
     (sizeof(type) <= 1024)? flt_finish_claim_1024((flt), (ptr)): \
     (ptr))

#define flt_release(flt, type, ptr) \
    ((sizeof(type) <= 8)? flt_release_8((flt), (ptr)): \
     (sizeof(type) <= 64)? flt_release_64((flt), (ptr)): \
     (sizeof(type) <= 256)? flt_release_256((flt), (ptr)): \
     (sizeof(type) <= 1024)? flt_release_1024((flt), (ptr)): \
     flt_dealloc_any((flt), sizeof(type), (ptr)))

#define flt_unused(flt, type) \
    ((sizeof(type) <= 8)? (flt)->unused8: \
     (sizeof(type) <= 64)? (flt)->unused64: \
     (sizeof(type) <= 256)? (flt)->unused256: \
     (sizeof(type) <= 1024)? (flt)->unused1024: \
     NULL)


/*-----------------------------------------------------------------------
 * Context-local data
 */

struct flt_local {
    void  *instances;
};


struct flt_local *
flt_local_new_size(struct flt *flt, size_t instance_size);

#define flt_local_new(flt, type) \
    flt_local_new_size((flt), sizeof(type))

void
flt_local_free(struct flt *flt, struct flt_local *local);

#define flt_local_get_shard_typed(flt, type, local, index) \
    ((type *) \
     ((char *) (local)->instances + \
      (index) * flt_round_to_cache_line(sizeof(type))))

#define flt_local_get_current_shard_typed(flt, type, local) \
    flt_local_get_shard_typed(flt, type, local, (flt)->index)

#define flt_local_shard_get_shard_typed(flt, type, shard, idx) \
    ((type *) \
     ((char *) (shard) + \
      flt_round_to_cache_line(sizeof(type)) * \
      (int) ((idx) - (flt)->index)))

#define flt_local_shard_migrate_typed(from, type, from_shard, to) \
    ((type *) \
     ((char *) (from_shard) + \
      flt_round_to_cache_line(sizeof(type)) * \
      (int) ((to)->index - (from)->index)))


void *
flt_local_get_current_shard(struct flt *flt, struct flt_local *local);

void *
flt_local_get_shard(struct flt *flt, struct flt_local *local,
                    unsigned int index);


#define flt_local_foreach(flt, type, local, i, inst) \
    for ((inst) = flt_local_get_shard_typed((flt), type, (local), (i) = 0); \
         (i) < (flt)->count; \
         (inst) = flt_local_get_shard_typed((flt), type, (local), ++(i))) \

#define flt_local_visit(flt, type, local, visit, ...) \
    do { \
        unsigned int  __i; \
        type  *__instance; \
        flt_local_foreach(flt, type, local, __i, __instance) { \
            (visit)((flt), __i, __instance, __VA_ARGS__); \
        } \
    } while (0)


#define flt_local_shard_foreach(flt, type, shard, i, inst) \
    for ((inst) = flt_local_shard_get_shard_typed \
            ((flt), type, (shard), (i) = 0); \
         (i) < (flt)->count; \
         (inst) = flt_local_shard_get_shard_typed \
            ((flt), type, (shard), ++(i))) \

#define flt_local_shard_visit(flt, type, shard, visit, ...) \
    do { \
        unsigned int  __i; \
        type  *__instance; \
        flt_local_shard_foreach(flt, type, shard, __i, __instance) { \
            (visit)((flt), __i, __instance, __VA_ARGS__); \
        } \
    } while (0)


/*-----------------------------------------------------------------------
 * Afters
 */

struct flt_after;
struct flt_after_shard;

struct flt_after *
flt_after_new(struct flt *flt, struct flt_task *trigger);

struct flt_after_shard *
flt_after_get_shard(struct flt *flt, struct flt_after *after,
                    unsigned int index);

void
flt_after_track(struct flt *flt, struct flt_after *after,
                struct flt_task *task);

void
flt_after_shard_track(struct flt *flt, struct flt_after_shard *shard,
                      struct flt_task *task);


#endif /* FLEET_H */
