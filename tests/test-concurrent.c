/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <check.h>

#include "fleet.h"

#include "helpers.h"
#include "fleet-test.c"


/*-----------------------------------------------------------------------
 * Test computation
 */

static uint64_t  min = 0;
static uint64_t  max = 1000000;
static uint64_t  result;

static void
run_native(void)
{
    volatile uint64_t  sum = 0;
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
schedule(struct flt *flt, void *u1, void *u2, void *u3, void *u4)
{
    uint64_t  i;
    for (i = min; i < max; i++) {
        flt_run(flt, add_one, (void *) i, NULL, NULL, NULL);
    }
}

static void
run_in_fleet(struct flt_fleet *fleet)
{
    result = 0;
    flt_fleet_run(fleet, schedule, NULL, NULL, NULL, NULL);
}

static void
verify(void)
{
    fail_unless_equal("Results", "%" PRIu64, UINT64_C(499999500000), result);
}


test_fleet_computation(concurrent);


/*-----------------------------------------------------------------------
 * Testing harness
 */

int
main(int argc, const char **argv)
{
    int  number_failed;
    Suite  *suite = test_suite();
    SRunner  *runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0)? EXIT_SUCCESS: EXIT_FAILURE;
}
