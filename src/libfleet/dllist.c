/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2011-2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include "fleet/dllist.h"


void
flt_dllist_init(struct flt_dllist *list)
{
    list->head.next = &list->head;
    list->head.prev = &list->head;
}


void
flt_dllist_map(struct flt_dllist *list, void *ud, flt_dllist_map_f *map)
{
    struct flt_dllist_item  *curr = flt_dllist_start(list);
    while (!flt_dllist_is_end(list, curr)) {
        /* Extract the next pointer now, just in case func frees the
         * list item. */
        struct flt_dllist_item  *next = curr->next;
        map(ud, curr);
        curr = next;
    }
}


size_t
flt_dllist_size(const struct flt_dllist *list)
{
    size_t  size = 0;
    struct flt_dllist_item  *curr;
    for (curr = flt_dllist_end(list);
         !flt_dllist_is_start(list, curr); curr = curr->prev) {
        size++;
    }
    return size;
}
