/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2011-2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef LIBFLT_DS_DLLIST_H
#define LIBFLT_DS_DLLIST_H

#include <stddef.h>

#include "fleet/internal.h"


struct flt_dllist_item {
    /* A pointer to the next element in the list. */
    struct flt_dllist_item  *next;
    /* A pointer to the previous element in the list. */
    struct flt_dllist_item  *prev;
};


struct flt_dllist {
    /* The sentinel element for this list. */
    struct flt_dllist_item  head;
};

#define FLT_DLLIST_INIT(list)  { { &(list).head, &(list).head } }

#define flt_dllist_init(list) \
    do { \
        (list)->head.next = &(list)->head; \
        (list)->head.prev = &(list)->head; \
    } while (0)


typedef void
flt_dllist_map_f(void *ud, struct flt_dllist_item *element);

FLT_INTERNAL
void
flt_dllist_map(struct flt_dllist *list, void *ud, flt_dllist_map_f *map);


FLT_INTERNAL
size_t
flt_dllist_size(const struct flt_dllist *list);


#define flt_dllist_add_after(pred, element) \
    do { \
        (element)->prev = (pred); \
        (element)->next = (pred)->next; \
        (pred)->next->prev = (element); \
        (pred)->next = (element); \
    } while (0)

#define flt_dllist_add_before(succ, element) \
    do { \
        (element)->next = (succ); \
        (element)->prev = (succ)->prev; \
        (succ)->prev->next = (element); \
        (succ)->prev = (element); \
    } while (0)

#define flt_dllist_add_to_head(list, element) \
    flt_dllist_add_after(&(list)->head, (element))

#define flt_dllist_add_to_tail(list, element) \
    flt_dllist_add_before(&(list)->head, (element))


#define flt_dllist_remove(element) \
    do { \
        (element)->prev->next = (element)->next; \
        (element)->next->prev = (element)->prev; \
    } while (0)


#define flt_dllist_is_empty(list) \
    (flt_dllist_is_end((list), flt_dllist_start((list))))


#define flt_dllist_head(list) \
    (((list)->head.next == &(list)->head)? NULL: (list)->head.next)
#define flt_dllist_tail(list) \
    (((list)->head.prev == &(list)->head)? NULL: (list)->head.prev)

#define flt_dllist_start(list) \
    ((list)->head.next)
#define flt_dllist_end(list) \
    ((list)->head.prev)

#define flt_dllist_is_start(list, element) \
    ((element) == &(list)->head)
#define flt_dllist_is_end(list, element) \
    ((element) == &(list)->head)


#define flt_dllist_add_list_to_head(dest, src) \
    do { \
        struct flt_dllist_item  *dest_end = flt_dllist_end(dest); \
        struct flt_dllist_item  *src_end = flt_dllist_end(src); \
        dest_end->next = &(src)->head; \
        src_end->next = &(dest)->head; \
        (src)->head.prev = dest_end; \
        (dest)->head.prev = src_end; \
        flt_dllist_remove(&(src)->head); \
        flt_dllist_init(src); \
    } while (0)


#endif /* LIBFLT_DS_DLLIST_H */
