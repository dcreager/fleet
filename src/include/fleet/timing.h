/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef FLEET_TIMING_H
#define FLEET_TIMING_H

#include "libcork/core.h"


#if !defined(FLT_MEASURE_TIMING)
#define FLT_MEASURE_TIMING  0
#endif


#if FLT_MEASURE_TIMING

#if defined(__linux__)
#include <time.h>

#elif defined(__MACH__)
#include <time.h>
#include <sys/time.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

#else
#error "Don't know how to calculate time on this platform!"
#endif

CORK_ATTR_UNUSED
static uint64_t
flt_get_time(void)
{
#if defined(__linux__)
    struct timespec  ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (ts.tv_nsec / 1000) + (ts.tv_sec * 1000000);

#elif defined(__MACH__)
    static mach_timebase_info_data_t    timebase;
    if (CORK_UNLIKELY(timebase.denom == 0)) {
        (void) mach_timebase_info(&timebase);
        timebase.denom *= 1000;
    }

    return mach_absolute_time() * timebase.numer / timebase.denom;

#else
#error "Don't know how to calculate time on this platform!"
#endif
}


CORK_ATTR_UNUSED
static void
flt_print_time(FILE *out, uint64_t usec)
{
    uint64_t  sec = usec / 1000000;
    usec = usec % 1000000;
    fprintf(out, "%" PRIu64 ".%06" PRIu64, sec, usec);
}


struct flt_stopwatch {
    uint64_t  last;
};

CORK_ATTR_UNUSED
static void
flt_stopwatch_start(struct flt_stopwatch *stopwatch)
{
    stopwatch->last = flt_get_time();
}

CORK_ATTR_UNUSED
static uint64_t
flt_stopwatch_get_delta(struct flt_stopwatch *stopwatch)
{
    uint64_t  last = stopwatch->last;
    uint64_t  curr = flt_get_time();
    stopwatch->last = curr;
    return curr - last;
}
#endif


#endif /* FLEET_TIMING_H */
