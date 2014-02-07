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
static unsigned long  result;

static void
configure(int argc, char **argv)
{
    if (argc != 1) {
        fprintf(stderr, "Usage: concurrent_unbatched [count]\n");
        exit(EXIT_FAILURE);
    }
    min = 0;
    max = flt_parse_ulong(argv[0]);
}

static void
print_name(FILE *out)
{
    fprintf(out, "concurrent_unbatched:%lu", max);
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

static flt_task  add_one;
static flt_task  merge_batches;
static flt_task  schedule;

static void
add_one(struct flt *flt, void *ud, size_t i)
{
    struct flt_local  *local = ud;
    unsigned long  *result = flt_local_get(flt, local, unsigned long);
    *result += i;
}

static void
merge_one_batch(struct flt *flt, unsigned long *batch_count, int dummy)
{
    result += *batch_count;
}

static void
merge_batches(struct flt *flt, void *ud, size_t i)
{
    struct flt_local  *local = ud;
    flt_local_visit(flt, local, unsigned long, merge_one_batch, 0);
    flt_local_free(flt, local);
}

static void
ulong_init(struct flt *flt, void *ud, void *vinstance)
{
}

static void
ulong_done(struct flt *flt, void *ud, void *vinstance)
{
}

static void
schedule(struct flt *flt, void *ud, size_t min)
{
    struct flt_local  *local;
    struct flt_task_group  *group;
    struct flt_task  *task;

    local = flt_local_new(flt, unsigned long, NULL, ulong_init, ulong_done);
    group = flt_task_group_new(flt);
    flt_task_group_run_after_current(flt, group);
    task = flt_task_new(flt, merge_batches, local, 0);
    flt_task_group_add(flt, group, task);

    task = flt_bulk_task_new(flt, add_one, local, min, max);
    flt_run(flt, task);
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
    unsigned long  expected = max / 2 * (max - 1);
    flt_check_result(concurrent_unbatched, "%lu", result, expected);
    return 0;
}

struct flt_example  concurrent_unbatched = {
    configure,
    print_name,
    run_native,
    run_in_fleet,
    verify
};
