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

static void
add_one(struct flt *flt, void *u1, void *u2, void *u3, void *u4)
{
    uint64_t  i = (uintptr_t) u1;
    result += i;
}

static void
schedule_batch(struct flt *flt, void *u1, void *u2, void *u3, void *u4)
{
    uint64_t  i = (uintptr_t) u1;
    uint64_t  j = i + batch_size;

    if (j > max) {
        j = max;
    } else {
        flt_run(flt, schedule_batch, (void *) j, NULL, NULL, NULL);
    }

    /* TODO: This only works in a single-threaded scheduler, since we're not
     * synchronizing updates to the result global variable. */
    for (; i < j; i++) {
        flt_run(flt, add_one, (void *) i, NULL, NULL, NULL);
    }
}

static void
run_in_fleet(struct flt_fleet *fleet)
{
    result = 0;
    flt_fleet_run(fleet, schedule_batch, (void *) min, NULL, NULL, NULL);
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
