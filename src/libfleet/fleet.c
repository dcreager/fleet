/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include "libcork/core.h"
#include "libcork/ds.h"

#include "fleet.h"
#include "fleet/task.h"

#if !defined(FLT_DEBUG)
#define FLT_DEBUG  0
#endif

#if FLT_DEBUG
static struct flt_spinlock  debug_lock = FLT_SPINLOCK_INIT();

#define DEBUG(flt, ...) \
    do { \
        flt_spinlock_lock(&debug_lock); \
        fprintf(stderr, "[%4u] ", (flt)->public.index); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        flt_spinlock_unlock(&debug_lock); \
    } while (0)
#else
#define DEBUG(flt, ...)  ((void) (flt))
#endif


/*-----------------------------------------------------------------------
 * Tasks
 */

/* 8Kb batches */
#define TASK_BATCH_SIZE  8192
#define TASK_BATCH_COUNT  (TASK_BATCH_SIZE / sizeof(struct flt_task))

/* Create a new batch of task instances.  Link them all together via their next
 * fields */
static struct flt_task *
flt_task_batch_new(struct flt_priv *flt)
{
    size_t  i;
    struct flt_task  *task = cork_malloc(TASK_BATCH_SIZE);
    struct flt_task  *first;
    struct flt_task  *curr;

    /* The first task in the batch is reserved, and is used to keep track of the
     * batches that are owned by this context. */
    cork_dllist_add_to_tail(&flt->batches, &task->item);

    /* This whole operation was kicked off because someone wants to create a new
     * task instance; the second instance is the batch is the one we'll use for
     * that. */
    first = task + 1;

    /* The remaining tasks in the batch can be used by the scheduler.  These
     * task instances start off local to this context, but might migrate to
     * other contexts while the scheduler runs.  That's perfectly fine; we keep
     * track of the batches separately so that we can free everything safely
     * when the fleet is freed. */
    for (i = 2, curr = first + 1; i < TASK_BATCH_COUNT; i++, curr++) {
        cork_dllist_add_to_tail(&flt->unused, &curr->item);
    }

    return first;
}

static void
flt_task_batch_free(struct flt_priv *flt, struct flt_task *batch)
{
    free(batch);
}

static struct flt_task *
flt_reuse_task(struct flt *pflt, const char *name, flt_task *func, void *ud,
               size_t min, size_t max);

static struct flt_task *
flt_create_task(struct flt *pflt, const char *name, flt_task *func, void *ud,
                size_t min, size_t max)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task  *task = flt_task_batch_new(flt);
    flt->public.new_task = flt_reuse_task;
    task->name = name;
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    task->group = NULL;
    return task;
}

static struct flt_task *
flt_reuse_task(struct flt *pflt, const char *name, flt_task *func, void *ud,
               size_t min, size_t max)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct cork_dllist_item  *head = cork_dllist_start(&flt->unused);
    struct flt_task  *task = cork_container_of(head, struct flt_task, item);
    if (CORK_UNLIKELY(cork_dllist_is_end(&flt->unused, head->next))) {
        flt->public.new_task = flt_create_task;
    }
    cork_dllist_remove(head);
    task->name = name;
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    task->group = NULL;
    return task;
}

static void
flt_task_free(struct flt_priv *flt, struct flt_task *task)
{
    cork_dllist_add_to_head(&flt->unused, &task->item);
    flt->public.new_task = flt_reuse_task;
}


/*-----------------------------------------------------------------------
 * Task groups
 */

static void
flt_task_group_ctx__init(struct flt *pflt, void *ud, void *vctx)
{
    struct flt_task_group_ctx  *ctx = vctx;
    ctx->group = ud;
    ctx->after = NULL;
    cork_dllist_init(&ctx->tasks);
    ctx->task_count = 0;
    ctx->execution_count = 0;
}

static void
flt_task_group_ctx__done(struct flt *pflt, void *ud, void *vctx)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group_ctx  *ctx = vctx;
    struct cork_dllist_item  *curr;
    struct cork_dllist_item  *next;
    struct flt_task  *task;

    /* If there any pending tasks that were never scheduled, free them now. */
    cork_dllist_foreach(&ctx->tasks, curr, next, struct flt_task, task, item) {
        flt_task_free(flt, task);
    }
}

struct flt_task_group *
flt_task_group_new(struct flt *pflt)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group  *group = cork_new(struct flt_task_group);
    DEBUG(flt, "New task group %p", group);
    group->ctxs = flt_local_new
        (&flt->public, struct flt_task_group_ctx, group,
         flt_task_group_ctx__init, flt_task_group_ctx__done);
    flt_counter_init(&group->active_ctx_count);
    group->next_after = NULL;
    group->state = FLT_TASK_GROUP_STOPPED;
    cork_dllist_add_to_head(&flt->groups, &group->item);
    return group;
}

static void
flt_task_group_free(struct flt_priv *flt, struct flt_task_group *group)
{
    flt_local_free(&flt->public, group->ctxs);
    free(group);
}

void
flt_task_group_start(struct flt *pflt, struct flt_task_group *group)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    unsigned int  i;
    struct flt_task_group_ctx  *ctx;

    DEBUG(flt, "Start task group %p", group);

    /* If this execution context didn't already have any tasks in its queue,
     * then the context just became active.  Bump the fleet's active context
     * count. */
    if (!flt->active) {
        DEBUG(flt, "Context is now active");
        flt_counter_inc(&flt->fleet->active_count);
        flt->active = true;
    }

    /* Move all of the group's pending tasks into the current execution
     * context's ready queue (regardless of which context they used to belong
     * to). */
    flt_local_foreach(pflt, group->ctxs, i, struct flt_task_group_ctx, ctx) {
        DEBUG(flt, "Start %zu/%zu tasks from group %p, context %u",
              ctx->task_count, ctx->execution_count, group, i);
        flt->execution_count += ctx->execution_count;
        cork_dllist_add_list_to_head(&flt->ready, &ctx->tasks);
    }
    group->state = FLT_TASK_GROUP_STARTED;
}

static void
flt_task_group_increment(struct flt_priv *flt, struct flt_task_group *group)
{
    struct flt_task_group_ctx  *ctx =
        flt_local_get(&flt->public, group->ctxs, struct flt_task_group_ctx);
    if (ctx->task_count++ == 0) {
        /* This is the first pending task that we've added to this per-context
         * object.  That means that the context has just become "active" for
         * this group, and we need to bump the group's active context count. */
        DEBUG(flt, "Group %p is now active for context %u",
              group, flt->public.index);
        flt_counter_inc(&group->active_ctx_count);
    }
}

static void
flt_task_group_fire_afters(struct flt_priv *flt, struct flt_task_group *group)
{
    size_t  i;
    struct flt_task_group_ctx  *ctx;
    flt_local_foreach(&flt->public, group->ctxs, i,
                      struct flt_task_group_ctx, ctx) {
        struct flt_task_group  *after;
        for (after = ctx->after; after != NULL; after = after->next_after) {
            flt_task_group_start(&flt->public, after);
        }
    }
}

static void
flt_task_group_decrement(struct flt_priv *flt, struct flt_task_group *group)
{
    struct flt_task_group_ctx  *ctx =
        flt_local_get(&flt->public, group->ctxs, struct flt_task_group_ctx);
    if (--ctx->task_count == 0) {
        DEBUG(flt, "Group %p has finished in context %u",
              group, flt->public.index);
        /* This task group has no more tasks in this context, so the context is
         * no longer active.  Decrement the active context count, and if *none*
         * of the contexts are active anymore, then start any task groups that
         * are supposed to execute after this group is done. */
        if (flt_counter_dec(&group->active_ctx_count)) {
            DEBUG(flt, "Group %p has finished", group);
            flt_task_group_fire_afters(flt, group);
        }
    }
}

void
flt_task_group_add(struct flt *pflt, struct flt_task_group *group,
                   struct flt_task *task)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group_ctx  *ctx =
        flt_local_get(pflt, group->ctxs, struct flt_task_group_ctx);
    size_t  count = task->max - task->min;
    task->group = group;
    cork_dllist_add_to_head(&ctx->tasks, &task->item);
    DEBUG(flt, "Add %s [%zu,%zu) to group %p",
          task->name, task->min, task->max, group);
    ctx->execution_count += count;
    flt_task_group_increment(flt, group);
}

void
flt_task_group_run_after(struct flt *pflt, struct flt_task_group *group,
                         struct flt_task_group *after)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group_ctx  *ctx =
        flt_local_get(pflt, group->ctxs, struct flt_task_group_ctx);
    DEBUG(flt, "Group %p will run after group %p", after, group);
    after->next_after = ctx->after;
    ctx->after = after;
}

void
flt_task_group_run_after_current(struct flt *pflt, struct flt_task_group *after)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group  *current_group = flt_current_group(flt);
    struct flt_task_group_ctx  *ctx =
        flt_local_get(pflt, current_group->ctxs, struct flt_task_group_ctx);
    DEBUG(flt, "Group %p will run after group %p", after, current_group);
    after->next_after = ctx->after;
    ctx->after = after;
}

static void
flt_task_group_move(struct flt_priv *flt, struct flt_task_group *group,
                    struct flt_priv *from)
{
    flt_task_group_decrement(from, group);
    flt_task_group_increment(flt, group);
}


/*-----------------------------------------------------------------------
 * Execution contexts
 */

static int
flt__thread_run(struct cork_thread_body *body);

static void
flt__thread_free(struct cork_thread_body *body)
{
    /* Nothing to do */
}

struct flt_priv *
flt_new(struct flt_fleet *fleet, size_t index, size_t count)
{
    struct flt_priv  *flt = cork_new(struct flt_priv);
    flt_spinlock_init(&flt->lock);
    flt->public.index = index;
    flt->public.count = count;
    flt->fleet = fleet;
    flt->public.new_task = flt_create_task;
    flt->execution_count = 0;
    cork_dllist_init(&flt->ready);
    cork_dllist_init(&flt->unused);
    cork_dllist_init(&flt->batches);
    cork_dllist_init(&flt->groups);
    flt->body.run = flt__thread_run;
    flt->body.free = flt__thread_free;
    flt->next_to_steal_from = (index + 1) % count;
    flt->waiting_to_steal = NULL;
    flt->active = false;
#if FLT_MEASURE_TIMING
    memset(&flt->timing, 0, sizeof(flt->timing));
#endif
    return flt;
}

static void
flt_task_batch_list_done(struct flt_priv *flt, struct cork_dllist *list)
{
    struct cork_dllist_item  *curr;
    struct cork_dllist_item  *next;
    struct flt_task  *task;
    cork_dllist_foreach(list, curr, next, struct flt_task, task, item) {
        flt_task_batch_free(flt, task);
    }
}

static void
flt_task_group_list_done(struct flt_priv *flt, struct cork_dllist *list)
{
    struct cork_dllist_item  *curr;
    struct cork_dllist_item  *next;
    struct flt_task_group  *group;
    cork_dllist_foreach(list, curr, next, struct flt_task_group, group, item) {
        flt_task_group_free(flt, group);
    }
}

void
flt_free(struct flt_priv *flt)
{
    flt_task_group_list_done(flt, &flt->groups);
    flt_task_batch_list_done(flt, &flt->batches);
    free(flt);
}

struct flt_task *
flt_current_task(struct flt_priv *flt)
{
    struct cork_dllist_item  *head = cork_dllist_start(&flt->ready);
    return cork_container_of(head, struct flt_task, item);
}

struct flt_task_group *
flt_current_group(struct flt_priv *flt)
{
    struct flt_task  *task = flt_current_task(flt);
    return task->group;
}


/*-----------------------------------------------------------------------
 * Fleet scheduler
 */

void
flt_run(struct flt *pflt, struct flt_task *task)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group  *current_group = flt_current_group(flt);
    struct flt_task_group_ctx  *ctx =
        flt_local_get(pflt, current_group->ctxs, struct flt_task_group_ctx);

    DEBUG(flt, "Add %s [%zu,%zu) to current group %p",
          task->name, task->min, task->max, current_group);
    task->group = current_group;

    /* The current task's group must already be running (otherwise how would we
     * have started the current task?), so we can add the task directly to the
     * context's ready queue, instead of adding it to the group. */
    cork_dllist_add_to_head(&flt->ready, &task->item);

    /* The current execution context must already be active for the current task
     * group (again, because there's already a task that was ready in this group
     * in this context).  So we never need to bump the groups active_ctx_count
     * field. */
    ctx->task_count++;
    flt->execution_count += (task->max - task->min);
}

void
flt_run_later(struct flt *pflt, struct flt_task *task)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group  *current_group = flt_current_group(flt);
    struct flt_task_group_ctx  *ctx =
        flt_local_get(pflt, current_group->ctxs, struct flt_task_group_ctx);

    DEBUG(flt, "Add %s [%zu,%zu) to end of current group %p",
          task->name, task->min, task->max, current_group);
    task->group = current_group;

    /* The current task's group must already be running (otherwise how would we
     * have started the current task?), so we can add the task directly to the
     * context's ready queue, instead of adding it to the group. */
    cork_dllist_add_to_tail(&flt->ready, &task->item);

    /* The current execution context must already be active for the current task
     * group (again, because there's already a task that was ready in this group
     * in this context).  So we never need to bump the groups active_ctx_count
     * field. */
    ctx->task_count++;
    flt->execution_count += (task->max - task->min);
}


#if FLT_MEASURE_TIMING
#define flt_start_stopwatch(flt)  flt_stopwatch_start(&(flt)->stopwatch)
#define flt_measure_time(flt, which) \
    (flt)->timing.which += flt_stopwatch_get_delta(&(flt)->stopwatch)
#else
#define flt_start_stopwatch(flt)      /* do nothing */
#define flt_measure_time(flt, which)  /* do nothing */
#endif

#define FLT_ROUND_SIZE  256

/* Returns the number of task iterations that were executed. */
static size_t
flt_pop_and_run_one(struct flt_priv *flt, size_t max_count)
{
    struct flt_task  *task = flt_current_task(flt);
    size_t  min = task->min;
    size_t  count = task->max - task->min;

    if (max_count < count) {
        /* There are more iterations in this bulk task than we can execute
         * during this lock acquisition.  So only execute the first max_count
         * iterations. */
        size_t  i;
        size_t  max = min + max_count;
        DEBUG(flt, "Run task %s [%zu,%zu)", task->name, min, max);
        for (i = min; i < max; i++) {
            flt_task_run(&flt->public, task, i);
        }
        task->min = max;
        flt->execution_count -= max_count;
        return max_count;
    } else {
        /* We can execute all of the iterations in this bulk task without
         * exceeding our allotment for this lock acquisition.  So execute them
         * all and retire the task. */
        size_t  i;
        size_t  max = task->max;
        DEBUG(flt, "Run task %s [%zu,%zu)", task->name, min, max);
        for (i = min; i < max; i++) {
            flt_task_run(&flt->public, task, i);
        }
        cork_dllist_remove(&task->item);
        flt_task_group_decrement(flt, task->group);
        flt_task_free(flt, task);
        flt->execution_count -= count;
        return count;
    }
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

/* Returns the number of tasks that we were able to steal. */
static size_t
flt_steal(struct flt_priv *flt)
{
    size_t  to_steal;
    size_t  left_to_steal;
    unsigned int  steal_index = flt_find_task_to_steal_from(flt);
    struct flt_priv  *steal_from = flt->fleet->contexts[steal_index];
    struct cork_dllist_item  *curr;
    struct cork_dllist_item  *prev;

    /* Is there anything to steal?  If not, give up. */
    if (steal_from->execution_count == 0) {
        DEBUG(flt, "Not going to steal from empty context %u", steal_index);
        return 0;
    }

    /* Is someone else already trying to steal from this context?  If so we have
     * to give up.  If not stake our claim. */
    if (cork_ptr_cas(&steal_from->waiting_to_steal, NULL, flt) != NULL) {
        return 0;
    }

    /* Grab the lock on the queue of the context we're going to steal from. */
    DEBUG(flt, "Steal from context %u", steal_index);
    flt_measure_time(flt, choosing_to_steal);
    flt_spinlock_lock(&flt->lock);
    flt_spinlock_lock(&steal_from->lock);
    flt_measure_time(flt, waiting_to_steal);

    /* And then steal! */
    to_steal = steal_from->execution_count / 2;
    left_to_steal = to_steal;
    DEBUG(flt, "Steal %zu tasks from context %u", to_steal, steal_index);

    /* We don't have to check for the beginning of the list, because we're
     * only going to try to steal half of the list.  We'll never make it to the
     * beginning. */
    for (curr = cork_dllist_end(&steal_from->ready), prev = curr->prev;
         left_to_steal > 0; curr = prev, prev = curr->prev) {
        struct flt_task  *task = cork_container_of(curr, struct flt_task, item);
        size_t  count = task->max - task->min;
        if (count > left_to_steal) {
            /* We only need to steal part of this bulk task to reach our theft
             * goal.  Create a new task instance to hold the portion that we
             * steal. */
            size_t  new_min = task->max - left_to_steal;
            struct flt_task  *new_task = flt->public.new_task
                (&flt->public, task->name, task->func,
                 task->ud, new_min, task->max);
            DEBUG(flt, "Steal %s [%zu,%zu) from context %u",
                  task->name, new_min, task->max, steal_index);
            task->max = new_min;
            new_task->group = task->group;
            flt_task_group_increment(flt, new_task->group);
            DEBUG(flt, "Leave %s [%zu,%zu) with context %u",
                  task->name, task->min, new_min, steal_index);
            cork_dllist_add_to_head(&flt->ready, &new_task->item);
            break;
        } else {
            /* We need to steal this entire task, and we'll need to steal more
             * to reach our goal. */
            DEBUG(flt, "Steal %s [%zu,%zu) from context %u",
                  task->name, task->min, task->max, steal_index);
            flt_task_group_move(flt, task->group, steal_from);
            cork_dllist_remove(&task->item);
            cork_dllist_add_to_head(&flt->ready, &task->item);
            left_to_steal -= count;
        }
    }

    /* Release the lock and return. */
    steal_from->execution_count -= to_steal;
    flt->execution_count += to_steal;
    DEBUG(flt, "Context %u now has %zu tasks",
          steal_index, steal_from->execution_count);
    DEBUG(flt, "Context %u now has %zu tasks",
          flt->public.index, flt->execution_count);
    steal_from->waiting_to_steal = NULL;
    flt_spinlock_unlock(&steal_from->lock);
    flt_spinlock_unlock(&flt->lock);
    flt_measure_time(flt, stealing);
    return to_steal;
}

static int
flt__thread_run(struct cork_thread_body *body)
{
    struct flt_priv  *flt = cork_container_of(body, struct flt_priv, body);
    unsigned int  spin_count;
    size_t  max_count;
    size_t  executed_count;

    flt_start_stopwatch(flt);
    if (cork_dllist_is_empty(&flt->ready)) {
        goto start_steal;
    } else {
        goto start_round;
    }

    /* Precondition: task queue unlocked, not empty */
start_round:
    /* If there is some other context waiting to steal from us, let them do
     * that before we execute anything else. */
    if (flt->waiting_to_steal != NULL) {
        DEBUG(flt, "Waiting to let someone steal from us");
        spin_count = 0;
        while (flt->waiting_to_steal != NULL) {
            flt_pause(spin_count);
        }
        flt_measure_time(flt, waiting_to_be_stolen_from);
    }

    /* Lock our task queue so that no one else can mess with it while we're
     * popping off tasks to execute.  After executing a maximum number of tasks,
     * we unlock the queue, even if we haven't fully drained the queue.  This
     * ensures that we don't starve any other threads that want to steal from
     * us. */

    flt_spinlock_lock(&flt->lock);
    flt_measure_time(flt, waiting_to_execute);
    DEBUG(flt, "Starting round");
    max_count = FLT_ROUND_SIZE;

    /* Precondition: task queue locked, not empty */
continue_round:
    /* We have at least one task in our queue to run. */
    executed_count = flt_pop_and_run_one(flt, max_count);
    max_count -= executed_count;

    if (CORK_UNLIKELY(cork_dllist_is_empty(&flt->ready))) {
        /* If we just drained the queue after executing this task, then this
         * context is no longer "active".  Decrement the fleet's active context
         * counter to see if we were the last active context.  If so, then the
         * whole fleet is done. */
        flt_measure_time(flt, executing);
        DEBUG(flt, "Executed %zu in round",
              (size_t) FLT_ROUND_SIZE - executed_count);
        flt->active = false;
        if (CORK_UNLIKELY(flt_counter_dec(&flt->fleet->active_count))) {
            DEBUG(flt, "Last context has run out of tasks");
            flt_spinlock_unlock(&flt->lock);
            return 0;
        } else {
            /* If we ran out of tasks, but there are other threads that still
             * have tasks to execute, let's try to steal some of them. */
            flt_spinlock_unlock(&flt->lock);
            goto start_steal;
        }
    } else if (max_count == 0) {
        /* We've executed the maximum number of tasks for this round, but there
         * are other tasks in the queue.  Unlock to see if anyone else wants to
         * steal from us, then start a new round. */
        flt_measure_time(flt, executing);
        DEBUG(flt, "Executed %zu in round", (size_t) FLT_ROUND_SIZE);
        flt_spinlock_unlock(&flt->lock);
        goto start_round;
    } else {
        /* We can keep executing tasks in this round. */
        goto continue_round;
    }

    /* Precondition: task unlocked, empty */
start_steal:
    DEBUG(flt, "Ran out of tasks");
    spin_count = 0;

    /* Precondition: task unlocked, empty */
steal:
    /* We don't have anything to execute.  First make sure that we haven't
     * completely run out of tasks. */
    if (CORK_UNLIKELY(flt_counter_get(&flt->fleet->active_count) == 0)) {
        flt_measure_time(flt, choosing_to_steal);
        DEBUG(flt, "All other contexts have run out of tasks");
        return 0;
    }

    /* Some thread out there still has some tasks to run; try to steal some for
     * ourselves. */
    if (flt_steal(flt)) {
        /* We got something!  Start a new round to execute these tasks. */
        flt_counter_inc(&flt->fleet->active_count);
        flt->active = true;
        goto start_round;
    } else {
        /* If we weren't able to steal anything, wait a bit and try again. */
        flt_pause(spin_count);
        goto steal;
    }
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
flt_fleet_run_(struct flt_fleet *fleet, const char *name,
               flt_task *func, void *ud, size_t index)
{
    struct flt_priv  *flt;
    struct flt_task_group  *group;
    struct flt_task  *task;
    unsigned int  i;

    if (CORK_UNLIKELY(fleet->contexts == NULL)) {
        flt_fleet_new_contexts(fleet);
    }

    flt = fleet->contexts[0];
    group = flt_task_group_new(&flt->public);
    task = flt->public.new_task(&flt->public, name, func, ud, index, index + 1);
    flt_task_group_add(&flt->public, group, task);
    flt_task_group_start(&flt->public, group);

    for (i = 0; i < fleet->count; i++) {
        cork_buffer_printf(&fleet->buf, "context.%u", i);
        flt = fleet->contexts[i];
        flt->thread = cork_thread_new(fleet->buf.buf, &flt->body);
        DEBUG(flt, "Start thread");
        cork_thread_start(flt->thread);
    }

    for (i = 0; i < fleet->count; i++) {
        flt = fleet->contexts[i];
        DEBUG(flt, "Wait for thread to finish");
        cork_thread_join(flt->thread);
        DEBUG(flt, "Thread finished");
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
        print_time(choosing_to_steal);
        print_time(executing);
        print_time(stealing);
        print_time(waiting_to_execute);
        print_time(waiting_to_steal);
        print_time(waiting_to_be_stolen_from);
    }
#endif
}
