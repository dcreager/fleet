/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef FLEET_EXAMPLES_H
#define FLEET_EXAMPLES_H

#include <stdio.h>

#include "fleet.h"


typedef void
flt_example_run_native(void);

typedef void
flt_example_run_in_fleet(struct flt_fleet *fleet);

/* Return 0 if everything ran correctly.  Print an error message to stderr and
 * return -1 if not. */
typedef int
flt_example_verify(void);

struct flt_example {
    flt_example_run_native  *run_native;
    flt_example_run_in_fleet  *run_in_fleet;
    flt_example_verify  *verify;
};


/* Runs an example in several configurations, and writes timing information to
 * `out` */
void
flt_run_example(FILE *out, const char *name, struct flt_example *example);


#define flt_check_result(test, fmt, expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "Unexpected result for " #test ":\n" \
                    "  got      " fmt "\n  expected " fmt "\n", \
                    (actual), (expected)); \
            return -1; \
        } \
    } while (0)


#endif /* FLEET_EXAMPLES_H */
