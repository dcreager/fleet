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
        fprintf(stderr, "Usage: sequential_return [count]\n");
        exit(EXIT_FAILURE);
    }
    min = 0;
    max = flt_parse_ulong(argv[0]);
}

static void
print_name(FILE *out)
{
    fprintf(out, "sequential_return:%lu", max);
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
        unsigned long  *result = ud;
        *result += i;
        return flt_return_to(flt, add_one, result, i+1);
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
    flt_check_result(sequential_return, "%lu", result, expected);
    return 0;
}

struct flt_example  sequential_return = {
    configure,
    print_name,
    run_native,
    run_in_fleet,
    verify
};
