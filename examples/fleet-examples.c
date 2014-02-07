/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "examples.h"

extern struct flt_example  concurrent_batched;
extern struct flt_example  concurrent_unbatched;
extern struct flt_example  sequential_return;
extern struct flt_example  sequential_run;

#define run_example(name, ...) \
    do { \
        static char  *argv[] = { __VA_ARGS__ }; \
        static int  argc = sizeof(argv) / sizeof(argv[0]); \
        flt_example_configure(&name, argc, argv); \
        flt_run_example(stderr, &name); \
    } while (0)

static void
run_all_examples(void)
{
    run_example(sequential_return, "100000000");
    run_example(sequential_run, "100000000");
    run_example(concurrent_unbatched, "100000000");
    run_example(concurrent_batched, "16", "100000000");
    run_example(concurrent_batched, "256", "100000000");
    run_example(concurrent_batched, "1024", "100000000");
}

#define run_named_example(name) \
    do { \
        if (strcmp(example_name, #name) == 0) { \
            flt_example_configure(&name, argc, argv); \
            flt_run_example_config(stderr, config, &name); \
            exit(EXIT_SUCCESS); \
        } \
    } while (0)

static void
run_named_examples(int argc, char **argv)
{
    const char  *config = (argc--, *(argv++));
    const char  *example_name = (argc--, *(argv++));
    (void) config;
    run_named_example(sequential_return);
    run_named_example(sequential_run);
    run_named_example(concurrent_unbatched);
    run_named_example(concurrent_batched);
    fprintf(stderr, "Unknown example %s\n", example_name);
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
    if (argc <= 1) {
        run_all_examples();
    } else if (argc == 2) {
        fprintf(stderr, "Usage: fleet-examples [config] [name] [options]\n");
        exit(EXIT_FAILURE);
    } else {
        run_named_examples(argc - 1, argv + 1);
    }
    return 0;
}
