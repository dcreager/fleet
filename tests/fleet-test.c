/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */


#define test_fleet_computation(test_name) \
\
START_TEST(test_##test_name##_native) \
{ \
    DESCRIBE_TEST; \
    run_native(); \
    verify(); \
} \
END_TEST \
\
START_TEST(test_##test_name##_single_threaded) \
{ \
    struct flt_fleet  *fleet; \
    DESCRIBE_TEST; \
    fleet = flt_fleet_new(); \
    run_in_fleet(fleet); \
    flt_fleet_free(fleet); \
    verify(); \
} \
END_TEST \
\
Suite * \
test_suite() \
{ \
    Suite  *s = suite_create("fleet"); \
    TCase  *tc_fleet = tcase_create(#test_name); \
    tcase_add_test(tc_fleet, test_##test_name##_native); \
    tcase_add_test(tc_fleet, test_##test_name##_single_threaded); \
    suite_add_tcase(s, tc_fleet); \
    return s; \
}
