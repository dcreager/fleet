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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "fleet.h"
#include "examples.h"


/* In microseconds */
static uint64_t
get_time(void)
{
    uint64_t  result;
    struct rusage  rusage;
    getrusage(RUSAGE_SELF, &rusage);
    result = rusage.ru_utime.tv_usec + 1000000 * rusage.ru_utime.tv_sec;
    result += rusage.ru_stime.tv_usec + 1000000 * rusage.ru_stime.tv_sec;
    return result;
}

static void
output_timing(FILE *out, const char *config, uint64_t usec)
{
    uint64_t  sec = usec / 1000000;
    usec = usec % 1000000;
    fprintf(out, "\t%s\t%" PRIu64 ".%06" PRIu64 "\n", config, sec, usec);
}

static void
output_error(FILE *out, const char *config)
{
    fprintf(out, "\t%s\tFAILED\n", config);
}


#define run(config, run) \
    do { \
        int  rc; \
        uint64_t  start; \
        uint64_t  end; \
        start = get_time(); \
        run; \
        end = get_time(); \
        example->print_name(out); \
        rc = example->verify(); \
        if (rc == 0) { \
            output_timing(out, config, end - start); \
        } else { \
            output_error(out, config); \
        } \
    } while (0)

static void
run_native(FILE *out, struct flt_example *example)
{
    run("native", example->run_native());
}

static void
run_single(FILE *out, struct flt_example *example)
{
    struct flt_fleet  *fleet = flt_fleet_new();
    run("single", example->run_in_fleet(fleet));
    flt_fleet_free(fleet);
}


void
flt_run_example(FILE *out, struct flt_example *example)
{
    run_native(out, example);
    run_single(out, example);
}


#define try_config(name) \
    do { \
        if (strcmp(#name, config) == 0) { \
            run_##name(out, example); \
            return; \
        } \
    } while (0)

void
flt_run_example_config(FILE *out, const char *config,
                       struct flt_example *example)
{
    try_config(native);
    try_config(single);
    fprintf(stderr, "Unknown config %s\n", config);
    exit(EXIT_FAILURE);
}
