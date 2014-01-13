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
output_timing(FILE *out, const char *name, const char *config, uint64_t usec)
{
    uint64_t  sec = usec / 1000000;
    usec = usec % 1000000;
    fprintf(out, "%s\t%s\t%" PRIu64 ".%06" PRIu64 "\n",
            name, config, sec, usec);
}

static void
output_error(FILE *out, const char *name, const char *config)
{
    fprintf(out, "%s\t%s\tFAILED\n", name, config);
}


#define run(config, run) \
    do { \
        int  rc; \
        uint64_t  start; \
        uint64_t  end; \
        start = get_time(); \
        run; \
        end = get_time(); \
        rc = example->verify(); \
        if (rc == 0) { \
            output_timing(out, name, config, end - start); \
        } else { \
            output_error(out, name, config); \
        } \
    } while (0)

static void
run_native(FILE *out, const char *name, struct flt_example *example)
{
    run("native", example->run_native());
}

static void
run_single(FILE *out, const char *name, struct flt_example *example)
{
    struct flt_fleet  *fleet = flt_fleet_new();
    run("single", example->run_in_fleet(fleet));
    flt_fleet_free(fleet);
}


void
flt_run_example(FILE *out, const char *name, struct flt_example *example)
{
    run_native(out, name, example);
    run_single(out, name, example);
}
