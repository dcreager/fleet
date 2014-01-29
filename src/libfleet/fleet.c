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
flt_reuse_task(struct flt *pflt, flt_task *func, void *ud,
               size_t min, size_t max);

static struct flt_task *
flt_create_task(struct flt *pflt, flt_task *func, void *ud,
                size_t min, size_t max)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task  *task = flt_task_batch_new(flt);
    flt->public.new_task = flt_reuse_task;
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    task->group = NULL;
    return task;
}

static struct flt_task *
flt_reuse_task(struct flt *pflt, flt_task *func, void *ud,
               size_t min, size_t max)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct cork_dllist_item  *head = cork_dllist_start(&flt->unused);
    struct flt_task  *task = cork_container_of(head, struct flt_task, item);
    if (CORK_UNLIKELY(cork_dllist_is_end(&flt->unused, head->next))) {
        flt->public.new_task = flt_create_task;
    }
    cork_dllist_remove(head);
    task->func = func;
    task->ud = ud;
    task->min = min;
    task->max = max;
    task->group = NULL;
    return task;
}

void
flt_task_free(struct flt_priv *flt, struct flt_task *task)
{
    cork_dllist_add_to_head(&flt->unused, &task->item);
    flt->public.new_task = flt_reuse_task;
}


/*-----------------------------------------------------------------------
 * Task groups
 */

static void *
flt_task_group_ctx__new(struct flt *pflt, void *ud)
{
    struct flt_task_group_ctx  *ctx = cork_new(struct flt_task_group_ctx);
    ctx->group = ud;
    ctx->after = NULL;
    cork_dllist_init(&ctx->tasks);
    ctx->task_count = 0;
    return ctx;
}

static void
flt_task_group_ctx__free(struct flt *pflt, void *ud, void *vctx)
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

    free(ctx);
}

struct flt_task_group *
flt_task_group_new(struct flt *pflt)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group  *group = cork_new(struct flt_task_group);
    group->ctxs = flt_local_new
        (&flt->public, group,
         flt_task_group_ctx__new, flt_task_group_ctx__free);
    flt_counter_init(&group->active_ctx_count);
    group->next_after = NULL;
    group->state = FLT_TASK_GROUP_STOPPED;
    cork_dllist_add_to_head(&flt->groups, &group->item);
    return group;
}

void
flt_task_group_free(struct flt_priv *flt, struct flt_task_group *group)
{
    flt_local_free(&flt->public, group->ctxs);
    free(group);
}

void
flt_task_group_start(struct flt *pflt, struct flt_task_group *group)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    size_t  i;
    struct flt_task_group_ctx  *ctx;
    /* Move all of the group's pending tasks into the current execution
     * context's ready queue (regardless of which context they used to belong
     * to). */
    flt_local_foreach(pflt, group->ctxs, i, ctx) {
        cork_dllist_add_list_to_head(&flt->ready, &ctx->tasks);
    }
    group->state = FLT_TASK_GROUP_STARTED;
}

void
flt_task_group_add(struct flt *pflt, struct flt_task_group *group,
                   struct flt_task *task)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group_ctx  *ctx = flt_local_get(&flt->public, group->ctxs);
    task->group = group;
    cork_dllist_add_to_head(&ctx->tasks, &task->item);
    if (ctx->task_count++ == 0) {
        /* This is the first pending task that we've added to this per-context
         * object.  That means that the context has just become "active" for
         * this group, and we need to bump the group's active context count. */
        flt_counter_inc(&group->active_ctx_count);
    }
}

void
flt_task_group_run_after(struct flt *pflt, struct flt_task_group *group,
                         struct flt_task_group *after)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group_ctx  *ctx = flt_local_get(&flt->public, group->ctxs);
    after->next_after = ctx->after;
    ctx->after = after;
}

void
flt_task_group_run_after_current(struct flt *pflt, struct flt_task_group *after)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group  *current_group = flt_current_group(flt);
    struct flt_task_group_ctx  *ctx = flt_local_get(pflt, current_group->ctxs);
    after->next_after = ctx->after;
    ctx->after = after;
}

static void
flt_task_group_fire_afters(struct flt_priv *flt, struct flt_task_group *group)
{
    size_t  i;
    struct flt_task_group_ctx  *ctx;
    flt_local_foreach(&flt->public, group->ctxs, i, ctx) {
        struct flt_task_group  *after;
        for (after = ctx->after; after != NULL; after = after->next_after) {
            flt_task_group_start(&flt->public, after);
        }
    }
}

void
flt_task_group_decrement(struct flt_priv *flt, struct flt_task_group *group)
{
    struct flt_task_group_ctx  *ctx = flt_local_get(&flt->public, group->ctxs);
    if (--ctx->task_count == 0) {
        /* This task group has no more tasks in this context, so the context is
         * no longer active.  Decrement the active context count, and if *none*
         * of the contexts are active anymore, then start any task groups that
         * are supposed to execute after this group is done. */
        if (flt_counter_dec(&group->active_ctx_count)) {
            flt_task_group_fire_afters(flt, group);
        }
    }
}


/*-----------------------------------------------------------------------
 * Execution contexts
 */

void
flt_init(struct flt_priv *flt, struct flt_fleet *fleet,
         size_t index, size_t count)
{
    flt_spinlock_init(&flt->lock);
    flt->public.index = index;
    flt->public.count = count;
    flt->fleet = fleet;
    flt->public.new_task = flt_create_task;
    cork_dllist_init(&flt->ready);
    cork_dllist_init(&flt->unused);
    cork_dllist_init(&flt->batches);
    cork_dllist_init(&flt->groups);
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
flt_done(struct flt_priv *flt)
{
    flt_task_group_list_done(flt, &flt->groups);
    flt_task_batch_list_done(flt, &flt->batches);
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
    struct flt_task_group_ctx  *ctx = flt_local_get(pflt, current_group->ctxs);

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
}

void
flt_run_later(struct flt *pflt, struct flt_task *task)
{
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_task_group  *current_group = flt_current_group(flt);
    struct flt_task_group_ctx  *ctx = flt_local_get(pflt, current_group->ctxs);

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
}


#define FLT_ROUND_SIZE  32

static size_t
flt_pop_and_run_one(struct flt_priv *flt, size_t max_count)
{
    struct flt_task  *task = flt_current_task(flt);
    size_t  max = task->min + max_count;

    if (max < task->max) {
        /* There are more iterations in this bulk task than we can execute
         * during this lock acquisition.  So only execute the first max_count
         * iterations. */
        size_t  i;
        for (i = task->min; i < max; i++) {
            flt_task_run(&flt->public, task, i);
        }
        task->min = max;
        return max_count;
    } else {
        /* We can execute all of the iterations in this bulk task without
         * exceeding our allotment for this lock acquisition.  So execute them
         * all and retire the task. */
        size_t  i;
        size_t  count;
        max = task->max;
        for (i = task->min; i < max; i++) {
            flt_task_run(&flt->public, task, i);
        }
        count = max - task->min;
        cork_dllist_remove(&task->item);
        flt_task_group_decrement(flt, task->group);
        flt_task_free(flt, task);
        return count;
    }
}

static void
flt_loop(struct flt_priv *flt)
{
    while (true) {
        size_t  max_count = FLT_ROUND_SIZE;
        flt_spinlock_lock(&flt->lock);
        while (max_count > 0) {
            if (cork_dllist_is_empty(&flt->ready)) {
                flt_spinlock_unlock(&flt->lock);
                return;
            } else {
                size_t  executed_count = flt_pop_and_run_one(flt, max_count);
                max_count -= executed_count;
            }
        }
        flt_spinlock_unlock(&flt->lock);
    }
}


struct flt_fleet *
flt_fleet_new(void)
{
    struct flt_fleet  *fleet = cork_new(struct flt_fleet);
    fleet->count = 1;
    fleet->contexts = cork_new(struct flt_priv);
    flt_init(fleet->contexts, fleet, 0, 1);
    return fleet;
}

void
flt_fleet_free(struct flt_fleet *fleet)
{
    flt_done(fleet->contexts);
    free(fleet->contexts);
    free(fleet);
}

void
flt_fleet_run(struct flt_fleet *fleet, flt_task *func, void *ud, size_t i)
{
    struct flt_priv  *flt = fleet->contexts;
    struct flt_task_group  *group = flt_task_group_new(&flt->public);
    struct flt_task  *task = flt_task_new(&flt->public, func, ud, i);
    flt_task_group_add(&flt->public, group, task);
    flt_task_group_start(&flt->public, group);
    flt_loop(flt);
}
