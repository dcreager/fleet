/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "fleet.h"
#include "examples.h"


static uint64_t  min;
static uint64_t  max;
static uint64_t  batch_size;
static uint64_t  result;

static void
run_native(void)
{
    uint64_t  sum = 0;
    uint64_t  i;
    for (i = min; i < max; i++) {
        sum += i;
    }
    result = sum;
}

static flt_task  add_one;
static flt_task  merge_batches;
static flt_task  schedule_batch;
static flt_task  schedule;

static void
add_one(struct flt *flt, void *ud, size_t i)
{
    struct flt_local  *local = ud;
    uint64_t  *result = flt_local_get(flt, local, uint64_t);
    *result += i;
}

static void
schedule_batch(struct flt *flt, void *ud, size_t i)
{
    struct flt_task  *task;
    struct flt_local  *local = ud;
    uint64_t  j = i + batch_size;

    if (j > max) {
        j = max;
    } else {
        task = flt_task_new(flt, schedule_batch, local, j);
        flt_run_later(flt, task);
    }

    task = flt_bulk_task_new(flt, add_one, local, i, j);
    flt_run(flt, task);
}

static void
merge_one_batch(struct flt *flt, uint64_t *batch_count, int dummy)
{
    result += *batch_count;
}

static void
merge_batches(struct flt *flt, void *ud, size_t i)
{
    struct flt_local  *local = ud;
    flt_local_visit(flt, local, uint64_t, merge_one_batch, 0);
    flt_local_free(flt, local);
}

static void
uint64_init(struct flt *flt, void *ud, void *vinstance)
{
}

static void
uint64_done(struct flt *flt, void *ud, void *vinstance)
{
}

static void
schedule(struct flt *flt, void *ud, size_t min)
{
    struct flt_local  *local;
    struct flt_task_group  *group;
    struct flt_task  *task;
    local = flt_local_new(flt, uint64_t, NULL, uint64_init, uint64_done);
    group = flt_task_group_new(flt);
    flt_task_group_run_after_current(flt, group);
    task = flt_task_new(flt, merge_batches, local, 0);
    flt_task_group_add(flt, group, task);
    return flt_return_to(flt, schedule_batch, local, min);
}

static void
run_in_fleet(struct flt_fleet *fleet)
{
    result = 0;
    flt_fleet_run(fleet, schedule, NULL, min);
}

static int
verify(void)
{
    uint64_t  expected = max * (max - 1) / 2;
    flt_check_result(concurrent_batched, "%" PRIu64, result, expected);
    return 0;
}

static struct flt_example  example = {
    run_native,
    run_in_fleet,
    verify
};

struct flt_example *
concurrent_batched(uint64_t batch_size_, uint64_t max_)
{
    result = 0;
    min = 0;
    max = max_;
    batch_size = batch_size_;
    return &example;
}
