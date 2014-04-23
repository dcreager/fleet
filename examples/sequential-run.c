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
        fprintf(stderr, "Usage: sequential_run [count]\n");
        exit(EXIT_FAILURE);
    }
    min = 0;
    max = flt_parse_ulong(argv[0]);
}

static void
print_name(FILE *out)
{
    fprintf(out, "sequential_run:%lu", max);
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

static void
add_one(struct flt *flt, struct flt_task *task)
{
    if (task->i < max) {
        unsigned long  *result = task->ud;
        *result += task->i;
        flt_task_new_scheduled(flt, add_one, result, task->i+1);
    }
}

static void
run_in_fleet(struct flt_fleet *fleet)
{
    result = 0;
    flt_fleet_run(fleet, add_one, &result, min);
}

static int
verify(void)
{
    unsigned long  expected = max / 2 * (max - 1);
    flt_check_result(sequential_run, "%lu", result, expected);
    return 0;
}

struct flt_example  sequential_run = {
    configure,
    print_name,
    run_native,
    run_in_fleet,
    verify
};
