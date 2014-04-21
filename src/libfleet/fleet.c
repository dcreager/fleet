/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "libcork/core.h"
#include "libcork/ds.h"
#include "libcork/threads.h"

#include "fleet.h"
#include "fleet/threads.h"
#include "fleet/timing.h"

#if !defined(FLT_DEBUG)
#define FLT_DEBUG  0
#endif

#if FLT_DEBUG
#include <unistd.h>
#define DEBUG(level, flt, ...) \
    do { \
        if ((level) <= FLT_DEBUG) { \
            cork_buffer_printf(&flt->debug, "[%4u] ", (flt)->public.index); \
            cork_buffer_append_printf(&flt->debug, __VA_ARGS__); \
            cork_buffer_append_literal(&flt->debug, "\n"); \
            write(STDERR_FILENO, flt->debug.buf, flt->debug.size); \
        } \
    } while (0)
#else
#define DEBUG(...)  /* do nothing */
#endif


/*-----------------------------------------------------------------------
 * Execution contexts
 */

#define NOBODY   ((struct flt_priv *) NULL)
#define BLOCKED  ((struct flt_priv *) 1)

#define NOTHING  ((struct flt_task_deque *) NULL)
#define WAITING  ((struct flt_task_deque *) 1)

struct flt_priv {
    struct flt  public;
    struct flt_fleet  *fleet;
    struct cork_thread  *thread;
    struct cork_thread_body  body;

    bool  active;
    struct flt_task_deque  *ready;
    struct flt_task_deque  *migrating;

    uint8_t  padding[FLT_CACHE_LINE_SIZE];

    unsigned int  next_to_steal_from;
    struct flt_priv * volatile  waiting_to_steal;
    struct flt_task_deque * volatile  receive;
    struct flt_task_deque * volatile  send;

#if FLT_DEBUG
    struct cork_buffer  debug;
#endif

#if FLT_MEASURE_TIMING
    struct flt_stopwatch  stopwatch;
    struct {
        uint64_t  executing;
        uint64_t  migrating;
        uint64_t  stealing;
        uint64_t  waiting_for_theft;
    } timing;
#endif
};

struct flt_fleet {
    struct flt_priv  **contexts;
    unsigned int  count;
    struct flt_counter  active_count;
    struct cork_buffer  buf;
};


/*-----------------------------------------------------------------------
 * Memory allocation
 */

void *
flt_alloc_8(struct flt *pflt, void *ptr)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    void  **next = malloc(8);
    *next = flt->public.unused8;
    flt->public.unused8 = next;
    return ptr;
}

void *
flt_alloc_64(struct flt *pflt, void *ptr)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    void  **next = malloc(64);
    *next = flt->public.unused64;
    flt->public.unused64 = next;
    return ptr;
}

void *
flt_alloc_256(struct flt *pflt, void *ptr)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    void  **next = malloc(256);
    *next = flt->public.unused256;
    flt->public.unused256 = next;
    return ptr;
}

void *
flt_alloc_1024(struct flt *pflt, void *ptr)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    void  **next = malloc(1024);
    *next = flt->public.unused1024;
    flt->public.unused1024 = next;
    return ptr;
}

void *
flt_alloc_any(struct flt *pflt, size_t size)
{
    return malloc(size);
}

void
flt_dealloc_any(struct flt *pflt, size_t size, void *ptr)
{
    free(ptr);
}


/*-----------------------------------------------------------------------
 * Tasks
 */

struct flt_task {
    struct flt_task  *next;
    flt_task_f  *func;
    void  *ud;
    size_t  i;
    flt_migrate_f  *migrate;
};

#define flt_task_run(flt, task) \
    ((task)->func(&(flt)->public, (task)->ud, (task)->i))

#define flt_task_migrate(from_ctx, to_ctx, task) \
    ((task)->migrate(&(from_ctx)->public, &(to_ctx)->public, \
                     (task)->ud, (task)->i))

struct flt_task_deque {
    struct flt_task  *tasks;
};

static struct flt_task_deque *
flt_task_deque_new(struct flt_priv *flt)
{
    struct flt_task_deque  *deque = cork_new(struct flt_task_deque);
    DEBUG(3, flt, "New task queue %p", deque);
    deque->tasks = NULL;
    return deque;
}

static void
flt_task_deque_free(struct flt_priv *flt, struct flt_task_deque *deque)
{
    struct flt_task  *curr;
    struct flt_task  *next;
    DEBUG(3, flt, "Free task queue %p", deque);
    for (curr = deque->tasks; curr != NULL; curr = next) {
        next = curr->next;
        free(curr);
    }
    free(deque);
}

static bool
flt_task_deque_is_empty(struct flt_priv *flt,
                        const struct flt_task_deque *deque)
{
    return deque->tasks == NULL;
}

static struct flt_task *
flt_task_deque_new_task(struct flt_priv *flt, struct flt_task_deque *deque)
{
    struct flt_task  *task = flt_claim(&flt->public, struct flt_task);
    task->next = deque->tasks;
    deque->tasks = task;
    return task;
}

static struct flt_task *
flt_task_deque_pop_head(struct flt_priv *flt, struct flt_task_deque *deque)
{
    struct flt_task  *task = deque->tasks;
    deque->tasks = task->next;
    DEBUG(3, flt, "Popped task from %p is %p(%p,%zu)",
          deque, task->func, task->ud, task->i);
    return task;
}


static void
flt_task_deque_migrate(struct flt_priv *from_ctx, struct flt_priv *to_ctx,
                       struct flt_task_deque *from, struct flt_task_deque *to)
{
    size_t  steal_count = 0;
    size_t  skip_count = 0;
    struct flt_task  *curr;
    struct flt_task  *last_to_stay;
    struct flt_task  *first_to_steal;

    /* First walk through the list, counting how many tasks should be skipped
     * and how many should be stolen.  For every two tasks that we walk through,
     * we bump the `first_to_steal` pointer by one.  The end result should be
     * that `first_to_steal` points at the task halfway through the linked list,
     * and that `last_to_stay` points to the task immediately before it, giving
     * us the dividing point between the tasks that stay and the tasks that
     * leave. */
    curr = from->tasks;
    last_to_stay = NULL;
    first_to_steal = curr;
    while (curr != NULL) {
        last_to_stay = first_to_steal;
        first_to_steal = first_to_steal->next;
        skip_count++;
        curr = curr->next;

        if (curr != NULL) {
            steal_count++;
            curr = curr->next;
        }
    }

    DEBUG(1, from_ctx, "Migrate %zu/%zu tasks into task queue %p",
          steal_count, steal_count + skip_count, to);

    /* Once we fall through, `first_to_steal` and `last_to_stay` should be set
     * correctly.  We know that to->tasks is currently NULL, since we can only
     * steal into an empty deque. */
    to->tasks = first_to_steal;
    if (last_to_stay == NULL) {
        /* The entire list should move. */
        from->tasks = NULL;
    } else {
        last_to_stay->next = NULL;
    }

    /* Lastly we have to step through all of the just-stolen tasks and call
     * their `migrate` callbacks, to let them update their `ud` parameters. */
    for (curr = first_to_steal; curr != NULL; curr = curr->next) {
        DEBUG(3, from_ctx,
              "Migrate task %p(%p,%zu) from %p to %p",
              curr->func, curr->ud, curr->i, from, to);
        curr->ud = flt_task_migrate(from_ctx, to_ctx, curr);
    }
}


/*-----------------------------------------------------------------------
 * Execution contexts
 */

static void *
flt_default_migrate(struct flt *from_ctx, struct flt *to_ctx,
                    void *ud, size_t i)
{
    return ud;
}

void
flt_run(struct flt *pflt, flt_task_f *func, void *ud, size_t i)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task  *task = flt_task_deque_new_task(flt, flt->ready);
    task->func = func;
    task->ud = ud;
    task->i = i;
    task->migrate = flt_default_migrate;
    DEBUG(3, flt, "New task in %p is %p(%p,%zu)",
          flt->ready, func, ud, i);
    flt_finish_claim(&flt->public, struct flt_task, task);
}

void
flt_run_migratable(struct flt *pflt, flt_task_f *func, void *ud, size_t i,
                   flt_migrate_f *migrate)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task  *task = flt_task_deque_new_task(flt, flt->ready);
    task->func = func;
    task->ud = ud;
    task->i = i;
    task->migrate = migrate;
    DEBUG(3, flt, "New task in %p is %p(%p,%zu)",
          flt->ready, func, ud, i);
    flt_finish_claim(&flt->public, struct flt_task, task);
}


#if FLT_MEASURE_TIMING
#define flt_start_stopwatch(flt)  flt_stopwatch_start(&(flt)->stopwatch)
#define flt_measure_time(flt, which) \
    (flt)->timing.which += flt_stopwatch_get_delta(&(flt)->stopwatch)
#else
#define flt_start_stopwatch(flt)      /* do nothing */
#define flt_measure_time(flt, which)  /* do nothing */
#endif

#define FLT_ROUND_SIZE  32

static void
flt_release_task(struct flt_priv *flt, struct flt_task *task)
{
    flt_release(&flt->public, struct flt_task, task);
}

static void
flt_pop_and_run_one(struct flt_priv *flt)
{
    struct flt_task  *task = flt_task_deque_pop_head(flt, flt->ready);
    DEBUG(3, flt, "Run task %p(%p,%zu)", task->func, task->ud, task->i);
    flt_task_run(flt, task);
    flt_release_task(flt, task);
}

static unsigned int
flt_find_task_to_steal_from(struct flt_priv *flt)
{
    unsigned int  result;
    do {
        result = flt->next_to_steal_from;
        flt->next_to_steal_from += 1;
        flt->next_to_steal_from %= flt->public.count;
    } while (result == flt->public.index);
    return result;
}

static void
flt_block_stealing(struct flt_priv *flt)
{
    struct flt_priv  *waiting_to_steal = flt->waiting_to_steal;

    /* Try to block other contexts from claiming our waiting_to_steal field.
     * This blocks everyone else from stealing from us. */
    if (waiting_to_steal == NOBODY) {
        waiting_to_steal = cork_ptr_cas
            (&flt->waiting_to_steal, NOBODY, BLOCKED);
        if (waiting_to_steal == NOBODY) {
            DEBUG(2, flt, "Blocked other contexts from stealing from us");
            return;
        }
    }

    /* If that failed, send a quick "nothing to transfer" message to the context
     * trying to steal from us. */
    DEBUG(1, flt, "Notify stealing context %u that we have nothing",
          waiting_to_steal->public.index);
    waiting_to_steal->receive = NOTHING;
    DEBUG(2, flt, "Block other contexts from stealing from us");
    flt->waiting_to_steal = BLOCKED;
}

/* Returns whether we successfully stole anything */
static bool
flt_steal(struct flt_priv *flt)
{
    unsigned int  steal_index;
    struct flt_priv  *already_stealing;
    struct flt_priv  *steal_from;
    struct flt_task_deque  *received;
    bool  success;

    /* Choose someone to steal from. */
    steal_index = flt_find_task_to_steal_from(flt);
    steal_from = flt->fleet->contexts[steal_index];

    /* Is there anything to steal?  If not, give up. */
    if (!steal_from->active) {
        DEBUG(1, flt, "Not going to steal from empty context %u", steal_index);
        return false;
    }

    /* Notify everyone that we want to steal from this context.  If someone else
     * is already stealing from this context, give up.  We will hopefully
     * receive a new ready queue from our target.  If so, the target will need a
     * replacement `migrating` queue.  Our ready queue is empty, so we can send
     * it to them as that replacement. */
    flt->send = flt->ready;
    flt->receive = WAITING;
    already_stealing = cork_ptr_cas(&steal_from->waiting_to_steal, NOBODY, flt);
    if (already_stealing != NOBODY) {
        if (already_stealing == BLOCKED) {
            DEBUG(1, flt, "No one can steal from context %u", steal_index);
        } else {
            DEBUG(1, flt, "Context %u is already stealing from %u",
                  already_stealing->public.index, steal_index);
        }
        return false;
    }
    flt_measure_time(flt, stealing);

    /* Wait until the other context has prepared a set of tasks for us to steal.
     * Once it has, it will fill in our `transfer` field. */
    DEBUG(1, flt, "Wait for tasks from context %u", steal_index);
    while (flt->receive == WAITING) {
        cork_pause();
        FLT_THREAD_YIELD();
    }
    flt_measure_time(flt, waiting_for_theft);

    /* We have a result! */
    received = flt->receive;
    success = (received != NOTHING);
    if (success) {
        DEBUG(1, flt, "Received tasks in queue %p from context %u",
              received, steal_index);
        flt->ready = received;
    } else {
        DEBUG(1, flt, "Context %u has nothing for us to steal", steal_index);
    }

    /* Unblock ourselves so that others can steal from us again. */
    DEBUG(2, flt, "Allow others to steal from us again");
    flt->waiting_to_steal = NOBODY;

    /* And we're done */
    return success;
}

static void
flt_send_to_stealer(struct flt_priv *flt)
{
    struct flt_priv  *waiting_to_steal = flt->waiting_to_steal;

    /* Is anyone trying to steal from us? */
    if (CORK_LIKELY(waiting_to_steal == NOBODY)) {
        return;
    }
    DEBUG(1, flt, "Context %u is stealing from us",
          waiting_to_steal->public.index);

    /* Do we have anything to send them? */
    if (flt_task_deque_is_empty(flt, flt->ready)) {
        DEBUG(1, flt, "We have nothing to steal");
        waiting_to_steal->receive = NOTHING;
    } else {
        struct flt_task_deque  *migrating = flt->migrating;
        struct flt_task_deque  *new_migrating = waiting_to_steal->send;

        DEBUG(2, flt, "Take task queue %p from stealer %u to replace queue %p",
              new_migrating, waiting_to_steal->public.index, migrating);

        /* Migrate some of our tasks into a temporary task queue. */
        flt_task_deque_migrate(flt, waiting_to_steal, flt->ready, migrating);

        /* And then send that temporary queue to the stealer. */
        waiting_to_steal->receive = migrating;
        flt->migrating = new_migrating;

        /* Bump the fleet's active_count now, to take incount that the receiving
         * context now has tasks to execute.  This ensures that if we happen to
         * drain our queue before the receiving task notices the tasks we just
         * sent them, we don't mistakenly think that all of the total work is
         * done. */
        flt_counter_inc(&flt->fleet->active_count);
    }

    /* Notify everyone that we can be stolen from again. */
    DEBUG(1, flt, "Signal that context %u is done stealing from us",
          waiting_to_steal->public.index);
    flt->waiting_to_steal = NOBODY;
    flt_measure_time(flt, migrating);
}

static int
flt__thread_run(struct cork_thread_body *body)
{
    struct flt_priv  *flt = cork_container_of(body, struct flt_priv, body);
    unsigned int  max_count;

    flt_start_stopwatch(flt);
    if (flt->active) {
        goto start_round;
    } else {
        goto start_steal;
    }

start_round:
    /* If there is some other context waiting to steal from us, let them do
     * that before we execute anything else. */
    flt_send_to_stealer(flt);
    DEBUG(2, flt, "Start round");
    max_count = FLT_ROUND_SIZE;

continue_round:
    if (CORK_UNLIKELY(flt_task_deque_is_empty(flt, flt->ready))) {
        /* If we just drained the queue, then this context is no longer active.
         * Decrement the fleet's active context counter to see if we were the
         * last active context.  If so, then the whole fleet is done. */
        flt_measure_time(flt, executing);
        DEBUG(0, flt, "Executed %zu in round",
              (size_t) FLT_ROUND_SIZE - max_count);
        flt->active = false;
        if (CORK_UNLIKELY(flt_counter_dec(&flt->fleet->active_count))) {
            DEBUG(1, flt, "Last context has run out of tasks");
            goto block_and_finish;
        } else {
            /* If we ran out of tasks, but there are other threads that still
             * have tasks to execute, let's try to steal some of them. */
            goto start_steal;
        }
    }

    /* We have at least one task in our queue to run. */
    flt_pop_and_run_one(flt);
    if (--max_count == 0) {
        /* We've executed the maximum number of tasks for this round, but there
         * are other tasks in the queue.  Check to see if there's anyone who
         * wants to steal from us, then start a new round. */
        flt_measure_time(flt, executing);
        DEBUG(2, flt, "Executed %zu in round", (size_t) FLT_ROUND_SIZE);
        goto start_round;
    } else {
        /* We can keep executing tasks in this round. */
        goto continue_round;
    }

    /* Precondition: task empty */
start_steal:
    DEBUG(2, flt, "Ran out of tasks");

    /* First prevent others from stealing from us. */
    flt_block_stealing(flt);

    /* Precondition: task empty */
steal:
    /* We don't have anything to execute.  First make sure that we haven't
     * completely run out of tasks. */
    if (CORK_UNLIKELY(flt_counter_get(&flt->fleet->active_count) == 0)) {
        DEBUG(1, flt, "All other contexts have run out of tasks");
        flt_measure_time(flt, stealing);
        goto finish;
    }

    /* Some thread out there still has some tasks to run; try to steal some for
     * ourselves. */
    if (flt_steal(flt)) {
        /* We got something!  Start a new round to execute these tasks. */
        flt->active = true;
        flt_measure_time(flt, stealing);
        goto start_round;
    } else {
        /* If we weren't able to steal anything, wait a bit and try again. */
        cork_pause();
        FLT_THREAD_YIELD();
        goto steal;
    }

block_and_finish:
    /* Before returning, if anyone is waiting for us to send them tasks, let
     * them know that we don't have any. */
    if (flt->waiting_to_steal != NOBODY) {
        DEBUG(1, flt, "Notify stealing context %u that we have nothing",
              flt->waiting_to_steal->public.index);
        flt->waiting_to_steal->receive = NOTHING;
        flt->waiting_to_steal = BLOCKED;
    }

finish:
    return 0;
}

static void
flt__thread_free(struct cork_thread_body *body)
{
    /* Nothing to do */
}

#define flt_alloc_initial(flt, sz) \
    do { \
        void  **next = malloc(sz); \
        *next = NULL; \
        flt->public.unused##sz = next; \
    } while (0)

static struct flt_priv *
flt_new(struct flt_fleet *fleet, size_t index, size_t count)
{
    struct flt_priv  *flt = cork_new(struct flt_priv);
    flt->public.index = index;
    flt->public.count = count;
    flt_alloc_initial(flt, 8);
    flt_alloc_initial(flt, 64);
    flt_alloc_initial(flt, 256);
    flt_alloc_initial(flt, 1024);
#if FLT_DEBUG
    cork_buffer_init(&flt->debug);
#endif
    flt->fleet = fleet;
    flt->ready = flt_task_deque_new(flt);
    flt->migrating = flt_task_deque_new(flt);
    flt->body.run = flt__thread_run;
    flt->body.free = flt__thread_free;
    flt->next_to_steal_from = (index + 1) % count;
    flt->waiting_to_steal = NOBODY;
    flt->receive = NULL;
    flt->send = NULL;
    flt->active = false;
#if FLT_MEASURE_TIMING
    memset(&flt->timing, 0, sizeof(flt->timing));
#endif
    return flt;
}

static void
flt_free_unused(struct flt_priv *flt, void **unused)
{
    void  **curr;
    void  **next;
    for (curr = unused; curr != NULL; curr = next) {
        next = *curr;
        free(curr);
    }
}

static void
flt_free(struct flt_priv *flt)
{
    flt_task_deque_free(flt, flt->ready);
    flt_task_deque_free(flt, flt->migrating);
    flt_free_unused(flt, flt->public.unused8);
    flt_free_unused(flt, flt->public.unused64);
    flt_free_unused(flt, flt->public.unused256);
    flt_free_unused(flt, flt->public.unused1024);
#if FLT_DEBUG
    cork_buffer_done(&flt->debug);
#endif
    free(flt);
}


static void
flt_fleet_new_contexts(struct flt_fleet *fleet)
{
    unsigned int  i;
    unsigned int  count = fleet->count;
    fleet->contexts = cork_calloc(count, sizeof(struct flt_priv *));
    for (i = 0; i < count; i++) {
        fleet->contexts[i] = flt_new(fleet, i, count);
    }
}

static void
flt_fleet_free_contexts(struct flt_fleet *fleet)
{
    unsigned int  i;
    unsigned int  count = fleet->count;
    for (i = 0; i < count; i++) {
        flt_free(fleet->contexts[i]);
    }
    free(fleet->contexts);
}

struct flt_fleet *
flt_fleet_new(void)
{
    struct flt_fleet  *fleet = cork_new(struct flt_fleet);
    fleet->count = flt_processor_count();
    fleet->contexts = NULL;
    flt_counter_init(&fleet->active_count);
    cork_buffer_init(&fleet->buf);
    return fleet;
}

void
flt_fleet_free(struct flt_fleet *fleet)
{
    if (fleet->contexts != NULL) {
        flt_fleet_free_contexts(fleet);
    }
    cork_buffer_done(&fleet->buf);
    free(fleet);
}

void
flt_fleet_set_context_count(struct flt_fleet *fleet, unsigned int context_count)
{
    if (fleet->contexts != NULL) {
        flt_fleet_free_contexts(fleet);
        fleet->contexts = NULL;
    }
    fleet->count = context_count;
}

void
flt_fleet_run(struct flt_fleet *fleet, flt_task_f *func, void *ud, size_t index)
{
    struct flt_priv  *flt;
    unsigned int  i;

    if (CORK_UNLIKELY(fleet->contexts == NULL)) {
        flt_fleet_new_contexts(fleet);
    }

    flt = fleet->contexts[0];
    flt_run(&flt->public, func, ud, index);
    flt->active = true;
    flt_counter_set(&fleet->active_count, 1);

    for (i = 0; i < fleet->count; i++) {
        cork_buffer_printf(&fleet->buf, "context.%u", i);
        flt = fleet->contexts[i];
        flt->thread = cork_thread_new(fleet->buf.buf, &flt->body);
        DEBUG(2, flt, "Start thread");
        cork_thread_start(flt->thread);
    }

    for (i = 0; i < fleet->count; i++) {
        flt = fleet->contexts[i];
        DEBUG(2, flt, "Wait for thread to finish");
        cork_thread_join(flt->thread);
        DEBUG(2, flt, "Thread finished");
        flt->thread = NULL;
    }

#if FLT_MEASURE_TIMING
#define print_time(which) \
    do { \
        fprintf(stderr, "       "); \
        flt_print_time(stderr, flt->timing.which); \
        fprintf(stderr, " " #which "\n"); \
    } while (0)

    for (i = 0; i < fleet->count; i++) {
        flt = fleet->contexts[i];
        fprintf(stderr, "[%4u] Timing:\n", i);
        print_time(executing);
        print_time(migrating);
        print_time(stealing);
        print_time(waiting_for_theft);
    }
#endif
}
