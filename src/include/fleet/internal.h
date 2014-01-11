/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef FLEET_INTERNAL_H
#define FLEET_INTERNAL_H

#include <stddef.h>


#define FLT_INTERNAL  __attribute__((__visibility__("hidden")))


#define container_of(field, struct_type, field_name) \
    ((struct_type *) (- offsetof(struct_type, field_name) + \
                      (void *) (field)))

#define likely(expr)    (__builtin_expect((expr), 1))
#define unlikely(expr)  (__builtin_expect((expr), 0))


#endif /* FLEET_INTERNAL_H */
