/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdint.h>

#include "examples.h"

#define run_example1(name, typ, param) \
    do { \
        struct flt_example *name(typ); \
        flt_run_example(stderr, #name ":" #param, name(param)); \
    } while (0)

#define run_example2(name, t1, p1, t2, p2) \
    do { \
        struct flt_example *name(t1, t2); \
        flt_run_example(stderr, #name ":" #p1 ":" #p2, name(p1, p2)); \
    } while (0)

int
main(void)
{
    run_example1(sequential_return, uint64_t, 100000000);
    run_example1(sequential_run, uint64_t, 100000000);
    run_example1(concurrent_unbatched, uint64_t, 100000000);
    run_example2(concurrent_batched, uint64_t, 16, uint64_t, 100000000);
    run_example2(concurrent_batched, uint64_t, 256, uint64_t, 100000000);
    run_example2(concurrent_batched, uint64_t, 1024, uint64_t, 100000000);
    return 0;
}
