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
#include <stdlib.h>

#include "fleet.h"


typedef void
flt_example_configure(int argc, char **argv);

typedef void
flt_example_print_name(FILE *out);

typedef void
flt_example_run_native(void);

typedef void
flt_example_run_in_fleet(struct flt_fleet *fleet);

/* Return 0 if everything ran correctly.  Print an error message to stderr and
 * return -1 if not. */
typedef int
flt_example_verify(void);

struct flt_example {
    flt_example_configure  *configure;
    flt_example_print_name  *print_name;
    flt_example_run_native  *run_native;
    flt_example_run_in_fleet  *run_in_fleet;
    flt_example_verify  *verify;
};

#define flt_example_configure(ex, argc, argv)  ((ex)->configure((argc), (argv)))


__attribute__((unused))
static unsigned long
flt_parse_ulong(const char *str)
{
    char  *endptr = NULL;
    unsigned long  result;

    if (str[0] == '\0') {
        fprintf(stderr, "Invalid integer \"\".");
        exit(EXIT_FAILURE);
    }
    result = strtoul(str, &endptr, 10);
    if (endptr[0] == '\0') {
        return result;
    } else {
        fprintf(stderr, "Invalid integer \"%s\".", str);
        exit(EXIT_FAILURE);
    }
}


/* Runs an example (which has already been configured) in several different
 * fleets, and writes timing information to `out`. */
void
flt_run_example_config(FILE *out, const char *config,
                       struct flt_example *example);

void
flt_run_example(FILE *out, struct flt_example *example);


#define flt_check_result(test, fmt, actual, expected) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "Unexpected result for " #test ":\n" \
                    "  got      " fmt "\n  expected " fmt "\n", \
                    (actual), (expected)); \
            return -1; \
        } \
    } while (0)


#endif /* FLEET_EXAMPLES_H */
