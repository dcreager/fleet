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


#define FLT_CACHE_LINE_SIZE  64


/*-----------------------------------------------------------------------
 * Spin locks
 */

#define FLT_SPINLOCK_AVAILABLE  ((unsigned int) -1)

struct flt_spinlock {
    union {
        volatile unsigned int  current_owner;
        uint8_t  padding[FLT_CACHE_LINE_SIZE];
    } _;
};

#define FLT_SPINLOCK_INIT()  { 0 }
#define flt_spinlock_init(lock) \
    do { \
        (lock)->_.current_owner = 0; \
    } while (0)

#define flt_pause(count) \
    do { \
        if (count < 10) { \
            /* Spin-wait */ \
            cork_pause(); \
        } else if (count < 20) { \
            /* A more intense spin-wait */ \
            int  i; \
            for (i = 0; i < 50; i++) { \
                cork_pause(); \
            } \
        } else if (count < 22) { \
            FLT_THREAD_YIELD(); \
        } else if (count < 24) { \
            usleep(0); \
        } else if (count < 50) { \
            usleep(1); \
        } else if (count < 75) { \
            usleep(count - 49 * 1000); \
        } else { \
            usleep(25000); \
        } \
        \
        count++; \
    } while (0)

#define flt_spinlock_lock(lock) \
    do { \
        unsigned int  count = 0; \
        unsigned int  prev; \
        do { \
            while (CORK_UNLIKELY((lock)->_.current_owner != 0)) { \
                flt_pause(count); \
            } \
            prev = cork_uint_cas(&(lock)->_.current_owner, 0, 1); \
        } while (CORK_UNLIKELY(prev != 0)); \
    } while (0)

#define flt_spinlock_unlock(lock) \
    do { \
        (lock)->_.current_owner = 0; \
    } while (0)


#endif /* FLEET_THREADS_H */
