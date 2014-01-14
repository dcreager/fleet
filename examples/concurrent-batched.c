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

#include "fleet.h"
#include "examples.h"


static uint64_t  min;
static uint64_t  max;
static uint64_t  batch_size;
static uint64_t  result;
static uint64_t  temp_result;

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
static flt_task  copy_result;
static flt_task  schedule_batch;
static flt_task  schedule;

static void
add_one(struct flt *flt, void *ud, size_t i)
{
    uint64_t  *result = ud;
    *result += i;
}

static void
copy_result(struct flt *flt, void *ud, size_t i)
{
    uint64_t  *temp_result = ud;
    result = *temp_result;
}

static void
schedule_batch(struct flt *flt, void *ud, size_t i)
{
    struct flt_task  *task;
    uint64_t  j = i + batch_size;

    if (j > max) {
        j = max;
    } else {
        task = flt_task_new(flt, schedule_batch, ud, j);
        flt_run_later(flt, task);
    }

    /* TODO: This only works in a single-threaded scheduler, since we're not
     * synchronizing updates to the result global variable. */
    task = flt_bulk_task_new(flt, add_one, ud, i, j);
    flt_run(flt, task);
}

static void
schedule(struct flt *flt, void *ud, size_t min)
{
    struct flt_task  *task = flt_task_new(flt, copy_result, ud, 0);
    flt_run_after_current_group(flt, task);
    return flt_return_to(flt, schedule_batch, ud, min);
}

static void
run_in_fleet(struct flt_fleet *fleet)
{
    result = 0;
    temp_result = 0;
    flt_fleet_run(fleet, schedule, &temp_result, min);
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
