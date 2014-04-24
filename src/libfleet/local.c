/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdlib.h>

#include "libcork/core.h"
#include "libcork/ds.h"

#include "fleet.h"


/*-----------------------------------------------------------------------
 * Context-local data
 */

/* We need to allocate one instance of the user's type for each execution
 * context.  To eliminate false sharing, we want to make sure that each user
 * instance is in a separate cache line.  We also need to allocate some shared
 * state that ties all of the instances together.  And lastly, we need to be
 * able to easily get to that shared state from any of the individual
 * per-context instance pointers.
 *
 * For now, we're going for a naïve approach, where we allocate a separate copy
 * of `flt_local` for each instance.  (We could allocate one `flt_local`, and
 * store a per-instance pointer to the single instance; however, because of the
 * cache-line alignment constraint, that wouldn't really save us any space.)
 *
 * So, we allocate a single large chunk of memory, containing interleaved
 * instances of `flt_local` and the user type:
 *
 *     +-------------+------------+-------------+------------+-----+
 *     | flt_local 0 | instance 0 | flt_local 1 | instance 1 | ... |
 *     +-------------+------------+-------------+------------+-----+
 *
 * To satisfy the cache-line alignment constraint, we round up the size of each
 * slot to the size of a cache line, and use posix_memalign() to make sure that
 * the entire chunk of memory is aligned to a cache line.
 */

struct flt_local {
    size_t  padded_element_size;
    void  *ud;
    flt_local_done_f  *done_instance;
};

#define PADDED_HEADER_SIZE  (flt_round_to_cache_line(sizeof(struct flt_local)))

struct flt_local *
flt_local_new_size(struct flt *flt, size_t instance_size, void *ud,
                   flt_local_init_f *init_instance,
                   flt_local_done_f *done_instance)
{
    unsigned int  i;
    void  *root;
    size_t  padded_instance_size = flt_round_to_cache_line(instance_size);
    size_t  padded_element_size = PADDED_HEADER_SIZE + padded_instance_size;
    size_t  full_size;
    char  *element;

    full_size = padded_element_size * flt->count;
    posix_memalign(&root, FLT_CACHE_LINE_SIZE, full_size);

    /* Initialize each of the user elements */
    element = root;
    for (i = 0; i < flt->count; i++, element += padded_element_size) {
        struct flt_local  *local = (struct flt_local *) element;
        void  *instance = element + PADDED_HEADER_SIZE;
        local->padded_element_size = padded_element_size;
        local->ud = ud;
        local->done_instance = done_instance;
        init_instance(flt, ud, instance, i);
    }

    return root;
}

static void
flt_local_root_free(struct flt *flt, void *root)
{
    size_t  i;
    struct flt_local  *local = root;
    size_t  padded_element_size = local->padded_element_size;
    char  *element;

    /* Finalize each of the user elements */
    element = root;
    for (i = 0; i < flt->count; i++, element += padded_element_size) {
        void  *instance = element + PADDED_HEADER_SIZE;
        local->done_instance(flt, local->ud, instance, i);
    }
    free(root);
}

void
flt_local_free(struct flt *flt, struct flt_local *local)
{
    flt_local_root_free(flt, local);
}

void
flt_local_shard_free(struct flt *flt, void *instance)
{
    char  *element = ((char *) instance) - PADDED_HEADER_SIZE;
    struct flt_local  *local = (struct flt_local *) element;
    size_t  padded_element_size = local->padded_element_size;
    char  *root = element - padded_element_size * flt->index;
    flt_local_root_free(flt, root);
}

void *
flt_local_get(struct flt *flt, struct flt_local *local)
{
    char  *root = (char *) local;
    size_t  padded_element_size = local->padded_element_size;
    return root + PADDED_HEADER_SIZE + padded_element_size * flt->index;
}

void *
flt_local_get_index(struct flt *flt, struct flt_local *local,
                    unsigned int index)
{
    char  *root = (char *) local;
    size_t  padded_element_size = local->padded_element_size;
    return root + PADDED_HEADER_SIZE + padded_element_size * index;
}

void *
flt_local_shard_get_index(struct flt *flt, void *instance, unsigned int index)
{
    char  *element = ((char *) instance) - PADDED_HEADER_SIZE;
    struct flt_local  *local = (struct flt_local *) element;
    size_t  padded_element_size = local->padded_element_size;
    return ((char *) instance) +
        padded_element_size * (int) (index - flt->index);
}

void *
flt_local_shard_migrate(struct flt *from, struct flt *to, void *from_instance)
{
    char  *element = ((char *) from_instance) - PADDED_HEADER_SIZE;
    struct flt_local  *local = (struct flt_local *) element;
    size_t  padded_element_size = local->padded_element_size;
    return ((char *) from_instance) +
        padded_element_size * (int) (to->index - from->index);
}
