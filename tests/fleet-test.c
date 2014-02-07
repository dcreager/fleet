/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include "examples.h"

#define test_fleet_computation(example, ...) \
\
START_TEST(test_native) \
{ \
    extern struct flt_example  example; \
    static char  *argv[] = { __VA_ARGS__ }; \
    static int  argc = sizeof(argv) / sizeof(argv[0]); \
    DESCRIBE_TEST; \
    example.configure(argc, argv); \
    example.run_native(); \
    fail_if(example.verify() != 0); \
} \
END_TEST \
\
START_TEST(test_single_threaded) \
{ \
    extern struct flt_example  example; \
    static char  *argv[] = { __VA_ARGS__ }; \
    static int  argc = sizeof(argv) / sizeof(argv[0]); \
    struct flt_fleet  *fleet; \
    DESCRIBE_TEST; \
    example.configure(argc, argv); \
    fleet = flt_fleet_new(); \
    example.run_in_fleet(fleet); \
    flt_fleet_free(fleet); \
    fail_if(example.verify() != 0); \
} \
END_TEST \
\
Suite * \
test_suite() \
{ \
    Suite  *s = suite_create("fleet"); \
    TCase  *tc_fleet = tcase_create("fleet"); \
    tcase_add_test(tc_fleet, test_native); \
    tcase_add_test(tc_fleet, test_single_threaded); \
    suite_add_tcase(s, tc_fleet); \
    return s; \
}
