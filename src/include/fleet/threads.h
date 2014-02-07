/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef FLEET_THREADS_H
#define FLEET_THREADS_H

#include <unistd.h>

#include "libcork/core.h"
#include "libcork/threads.h"

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif

#if defined(__APPLE__)
#include <pthread.h>
#define FLT_THREAD_YIELD   pthread_yield_np
#elif defined(__linux__) || defined(BSD)
#include <sched.h>
#define FLT_THREAD_YIELD   sched_yield
#else
#error "Unknown hybrid yield implementation"
#endif


#define flt_pause(count) \
    do { \
        if ((count) < 10) { \
            /* Spin-wait */ \
            cork_pause(); \
        } else if ((count) < 20) { \
            /* A more intense spin-wait */ \
            unsigned int  __i; \
            for (__i = 0; __i < 50; __i++) { \
                cork_pause(); \
            } \
        } else if ((count) < 22) { \
            FLT_THREAD_YIELD(); \
        } else if ((count) < 24) { \
            usleep(0); \
        } else if ((count) < 50) { \
            usleep(1); \
        } else if ((count) < 75) { \
            usleep(((count) - 49) * 1000); \
        } else { \
            usleep(25000); \
        } \
        \
        (count)++; \
    } while (0)


/*-----------------------------------------------------------------------
 * Memory barriers
 */

/* __sync_synchronize doesn't emit a memory fence instruction in GCC <= 4.3.  It
 * also implements a full memory barrier.  So, on x86_64, which has separate
 * lfence and sfence instructions, we use hard-coded assembly, regardless of GCC
 * version.  On i386, we can't guarantee that lfence/sfence are available, so we
 * have to use a full memory barrier anyway.  We use the GCC intrinsics if we
 * can; otherwise, we fall back on assembly.
 *
 * [1] http://gcc.gnu.org/bugzilla/show_bug.cgi?id=36793
 */

#if defined(__GNUC__) && defined(__x86_64__)

CORK_ATTR_UNUSED
static inline void
flt_read_barrier(void)
{
    __asm__ __volatile__ ("lfence" ::: "memory");
}

CORK_ATTR_UNUSED
static inline void
flt_write_barrier(void)
{
    __asm__ __volatile__ ("sfence" ::: "memory");
}

#elif (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) > 40300

CORK_ATTR_UNUSED
static inline void
flt_read_barrier(void)
{
    __sync_synchronize();
}

CORK_ATTR_UNUSED
static inline void
flt_write_barrier(void)
{
    __sync_synchronize();
}

#elif defined(__GNUC__) && defined(__i386__)

CORK_ATTR_UNUSED
static inline void
flt_read_barrier(void)
{
    int  a = 0;
    __asm__ __volatile__ ("lock orl $0, %0" : "+m" (a));
}

CORK_ATTR_UNUSED
static inline void
flt_write_barrier(void)
{
    int  a = 0;
    __asm__ __volatile__ ("lock orl $0, %0" : "+m" (a));
}

#else
#error "No memory barrier implementation!"
#endif


/*-----------------------------------------------------------------------
 * Cache-line-padded integers
 */

/* Add a full cache line of space both before and after the actual integer
 * value.  Depending on the alignment of the containing structure, the `value`
 * field might cross a cache line boundary, but we can at least guarantee that
 * any cache line that contains `value` doesn't contain anything else. */
struct flt_padded_uint {
    uint8_t  pre_padding[FLT_CACHE_LINE_SIZE];
    volatile unsigned int  value;
    uint8_t  post_padding[FLT_CACHE_LINE_SIZE];
};

#define FLT_PADDED_UINT_INIT()  {{},0,{}}

#define flt_padded_uint_cas(pui, oldv, newv) \
    (cork_uint_cas(&(pui)->_.value, (oldv), (newv)))

/* Not thread-safe */
#define flt_padded_uint_set_fast(pui, v) \
    ((pui)->value = (v))

#define flt_padded_uint_set(pui, v) \
    ((pui)->value = (v), flt_write_barrier())


/*-----------------------------------------------------------------------
 * Counters
 */

struct flt_counter {
    struct flt_padded_uint  value;
};

#define FLT_COUNTER_INIT()  {FLT_PADDED_UINT_INIT()}
#define flt_counter_init(ctr) \
    do { \
        (ctr)->value.value = 0; \
    } while (0)

/* Returns true if the decrement brings the counter to 0. */
#define flt_counter_dec(ctr) \
    (cork_uint_atomic_sub(&(ctr)->value.value, 1) == 0)

#define flt_counter_inc(ctr) \
    ((void) cork_uint_atomic_add(&(ctr)->value.value, 1))

#define flt_counter_get(ctr)  ((ctr)->value.value)

/* Not thread-safe */
#define flt_counter_set(ctr, v) \
    ((ctr)->value.value = (v))


/*-----------------------------------------------------------------------
 * Spin locks
 */

#define FLT_SPINLOCK_AVAILABLE  ((unsigned int) -1)

struct flt_spinlock {
    struct flt_padded_uint  current_owner;
};

#define FLT_SPINLOCK_INIT()  {FLT_PADDED_UINT_INIT()}
#define flt_spinlock_init(lock) \
    do { \
        (lock)->current_owner.value = 0; \
    } while (0)

#define flt_spinlock_lock(lock) \
    do { \
        unsigned int  count = 0; \
        unsigned int  prev; \
        do { \
            while (CORK_UNLIKELY((lock)->current_owner.value != 0)) { \
                flt_pause(count); \
            } \
            prev = cork_uint_cas(&(lock)->current_owner.value, 0, 1); \
        } while (CORK_UNLIKELY(prev != 0)); \
    } while (0)

#define flt_spinlock_unlock(lock) \
    do { \
        (lock)->current_owner.value = 0; \
    } while (0)


/*-----------------------------------------------------------------------
 * Detecting the number of CPUs
 */

#if (defined(__unix__) || defined(unix)) && !defined(USG)
/* We need this to test for BSD, but it's a good idea to have for
 * any brand of Unix.*/
#include <sys/param.h>
#endif

#if defined(__linux)

CORK_ATTR_UNUSED
static unsigned int
flt_processor_count(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

#elif defined(__APPLE__) && defined(__MACH__)

CORK_ATTR_UNUSED
static unsigned int
flt_processor_count(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

#elif defined(BSD) && (BSD >= 199103)

#include <sys/types.h>
#include <sys/sysctl.h>

CORK_ATTR_UNUSED
static unsigned int
flt_processor_count(void)
{
    unsigned int  processor_count;
    int  mib[4];
    size_t  len = sizeof(processor_count);

    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU;
    sysctl(mib, 2, &processor_count, &len, NULL, 0);

    if (processor_count < 1) {
        mib[1] = HW_NCPU;
        sysctl(mib, 2, &processor_count, &len, NULL, 0);
        if (processor_count < 1) {
            processor_count = 1;
        }
    }

    return processor_count;
}
#endif  /* platforms */

#endif /* FLEET_THREADS_H */
