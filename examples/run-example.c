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


struct timing {
    uint64_t  wall;
    uint64_t  cpu;
};

/* In microseconds */
static void
get_time(struct timing *timing)
{
    struct rusage  rusage;
    struct timeval  tv;
    getrusage(RUSAGE_SELF, &rusage);
    timing->cpu = rusage.ru_utime.tv_usec + 1000000 * rusage.ru_utime.tv_sec;
    timing->cpu += rusage.ru_stime.tv_usec + 1000000 * rusage.ru_stime.tv_sec;
    gettimeofday(&tv, NULL);
    timing->wall = tv.tv_usec + 1000000 * tv.tv_sec;
}

static void
diff_time(struct timing *start, struct timing *end)
{
    end->cpu -= start->cpu;
    end->wall -= start->wall;
}

static void
output_usec(FILE *out, uint64_t usec)
{
    uint64_t  sec = usec / 1000000;
    usec = usec % 1000000;
    fprintf(out, "%" PRIu64 ".%06" PRIu64, sec, usec);
}

static void
output_timing(FILE *out, const char *config, struct timing *timing)
{
    double  speedup;
    fprintf(out, "\t%s\t", config);
    output_usec(out, timing->cpu);
    fprintf(out, "\t");
    output_usec(out, timing->wall);
    speedup = ((double) timing->cpu) / ((double) timing->wall);
    fprintf(out, "\t%.2lf\n", speedup);
}

static void
output_error(FILE *out, const char *config)
{
    fprintf(out, "\t%s\tFAILED\n", config);
}


#define run(config, run) \
    do { \
        int  rc; \
        struct timing  start; \
        struct timing  end; \
        get_time(&start); \
        run; \
        get_time(&end); \
        rc = example->verify(); \
        example->print_name(out); \
        if (rc == 0) { \
            diff_time(&start, &end); \
            output_timing(out, config, &end); \
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
    flt_fleet_set_context_count(fleet, 1);
    run("single", example->run_in_fleet(fleet));
    flt_fleet_free(fleet);
}

static void
run_2core(FILE *out, struct flt_example *example)
{
    struct flt_fleet  *fleet = flt_fleet_new();
    flt_fleet_set_context_count(fleet, 2);
    run("2core", example->run_in_fleet(fleet));
    flt_fleet_free(fleet);
}

static void
run_4core(FILE *out, struct flt_example *example)
{
    struct flt_fleet  *fleet = flt_fleet_new();
    flt_fleet_set_context_count(fleet, 4);
    run("4core", example->run_in_fleet(fleet));
    flt_fleet_free(fleet);
}


void
flt_run_example(FILE *out, struct flt_example *example)
{
    run_native(out, example);
    run_single(out, example);
    run_2core(out, example);
    run_4core(out, example);
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
    try_config(2core);
    try_config(4core);
    fprintf(stderr, "Unknown config %s\n", config);
    exit(EXIT_FAILURE);
}
