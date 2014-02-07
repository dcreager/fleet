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

static flt_task  add_one;

static void
add_one(struct flt *flt, void *ud, size_t i)
{
    if (i < max) {
        struct flt_task  *task;
        unsigned long  *result = ud;
        *result += i;
        task = flt_task_new(flt, add_one, result, i+1);
        flt_run(flt, task);
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
    unsigned long  expected = max * (max - 1) / 2;
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
