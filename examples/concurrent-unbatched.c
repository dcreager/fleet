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
static flt_task  schedule;

static void
add_one(struct flt *flt, void *ud)
{
    uint64_t  i = (uintptr_t) ud;
    result += i;
}

static void
schedule(struct flt *flt, void *ud)
{
    /* TODO: This only works in a single-threaded scheduler, since we're not
     * synchronizing updates to the result global variable. */
    uint64_t  i;
    for (i = min; i < max; i++) {
        flt_run(flt, add_one, (void *) i);
    }
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
    uint64_t  expected = max * (max - 1) / 2;
    flt_check_result(concurrent_unbatched, "%" PRIu64, result, expected);
    return 0;
}

static struct flt_example  example = {
    run_native,
    run_in_fleet,
    verify
};

struct flt_example *
concurrent_unbatched(uint64_t max_)
{
    result = 0;
    min = 0;
    max = max_;
    return &example;
}
