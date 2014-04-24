/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>

#include "fleet.h"
#include "examples.h"


static unsigned long  min;
static unsigned long  max;
static unsigned long  batch_size;
static unsigned long  result;

static void
configure(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: concurrent_batched [batch size] [count]\n");
        exit(EXIT_FAILURE);
    }
    min = 0;
    max = flt_parse_ulong(argv[1]);
    batch_size = flt_parse_ulong(argv[0]);
}

static void
print_name(FILE *out)
{
    fprintf(out, "concurrent_batched:%lu:%lu", batch_size, max);
}

static void
run_native(void)
{
    unsigned long  sum = 0;
    unsigned long  i;
    for (i = min; i < max; i++) {
        sum += i;
    }
    result = sum;
}

static flt_task_run_f  add_one;
static flt_task_run_f  merge_batches;
static flt_task_run_f  schedule_one;
static flt_task_run_f  schedule;

struct add_shard {
    struct flt_scounter_ctx  *counter_ctx;
    unsigned long  result;
};

struct add_state {
    struct add_shard  *shard;
    unsigned long  i;
};

static void
add_shard__migrate(struct flt *from, struct flt *to,
                   struct flt_task *task, void *ud)
{
    struct add_state  *from_state = task->ud;
    struct add_shard  *from_shard = from_state->shard;
    flt_scounter_ctx_migrate(from, to, from_shard->counter_ctx);
    from_state->shard = flt_local_ctx_migrate(from, to, from_shard);
}


static void
merge_one_batch(struct flt *flt, unsigned int index, struct add_shard *shard,
                int dummy)
{
    result += shard->result;
}

static void
merge_batches(struct flt *flt, struct flt_task *task)
{
    struct add_state  *shard = task->ud;
    flt_local_ctx_visit(flt, struct add_shard, shard, merge_one_batch, 0);
    flt_local_ctx_free(flt, shard);
}

static void
add_one(struct flt *flt, struct flt_task *task)
{
    struct add_state  *state = task->ud;
    struct add_shard  *shard = state->shard;
    unsigned long  i = state->i;
    shard->result += i;
    if (flt_scounter_ctx_dec(flt, shard->counter_ctx)) {
        flt_task_new_scheduled(flt, merge_batches, shard);
    }
}

static void
add_state__finish(struct flt *flt, struct flt_task *task, void *ud)
{
    flt_release(flt, struct add_state, task->ud);
}

static void
schedule_one(struct flt *flt, struct flt_task *task)
{
    struct add_state  *state = task->ud;
    struct add_shard  *shard = state->shard;
    unsigned long  i = state->i;
    unsigned long  j = i + batch_size;

    if (j > max) {
        j = max;
    } else {
        flt_scounter_ctx_inc(flt, shard->counter_ctx);
        state->i = j;
        flt_task_reschedule(flt, task);
    }

    for (; i < j; i++) {
        struct add_state  *new_state;
        flt_scounter_ctx_inc(flt, shard->counter_ctx);
        new_state = flt_claim(flt, struct add_state);
        new_state->shard = shard;
        new_state->i = i;
        new_state = flt_finish_claim(flt, struct add_state, new_state);
        task = flt_task_new_scheduled(flt, add_one, new_state);
        flt_task_add_on_migrate(flt, task, add_shard__migrate, NULL);
        flt_task_add_on_finished(flt, task, add_state__finish, NULL);
    }

    if (flt_scounter_ctx_dec(flt, shard->counter_ctx)) {
        flt_task_new_scheduled(flt, merge_batches, shard);
    }
}

static void
add_shard__init(struct flt *flt, void *ud, void *vinstance, unsigned int index)
{
    struct flt_scounter  *counter = ud;
    struct add_shard  *shard = vinstance;
    shard->counter_ctx = flt_scounter_get(flt, counter, index);
    shard->result = 0;
}

static void
add_shard__done(struct flt *flt, void *ud, void *vinstance, unsigned int index)
{
}

static void
schedule(struct flt *flt, struct flt_task *task)
{
    struct flt_scounter  *counter;
    struct flt_local  *local;
    struct add_shard  *shard;
    struct add_state  *state;
    counter = flt_scounter_new(flt);
    local = flt_local_new
        (flt, struct add_shard, counter, add_shard__init, add_shard__done);
    shard = flt_local_get(flt, local);
    flt_scounter_inc(flt, counter);
    state = flt_claim(flt, struct add_state);
    state->shard = shard;
    state->i = min;
    state = flt_finish_claim(flt, struct add_state, state);
    task = flt_task_new_scheduled(flt, schedule_one, state);
    flt_task_add_on_migrate(flt, task, add_shard__migrate, NULL);
    flt_task_add_on_finished(flt, task, add_state__finish, NULL);
}

static void
run_in_fleet(struct flt_fleet *fleet)
{
    result = 0;
    flt_fleet_run(fleet, schedule, NULL);
}

static int
verify(void)
{
    unsigned long  expected = max / 2 * (max - 1);
    flt_check_result(concurrent_batched, "%lu", result, expected);
    return 0;
}

struct flt_example  concurrent_batched = {
    configure,
    print_name,
    run_native,
    run_in_fleet,
    verify
};
