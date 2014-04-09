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

struct flt_local_priv {
    struct flt_local  public;
    size_t  padded_instance_size;
};

#define PADDED_HEADER_SIZE \
    (flt_round_to_cache_line(sizeof(struct flt_local_priv)))

struct flt_local *
flt_local_new_size(struct flt *flt, size_t instance_size)
{
    void  *root;
    struct flt_local_priv  *local;
    size_t  padded_instance_size = flt_round_to_cache_line(instance_size);
    size_t  full_size;
    char  *instance;

    full_size = PADDED_HEADER_SIZE + padded_instance_size * flt->count;
    posix_memalign(&root, FLT_CACHE_LINE_SIZE, full_size);

    /* Initialize the flt_local structure */
    local = root;
    local->padded_instance_size = padded_instance_size;
    instance = ((char *) root) + PADDED_HEADER_SIZE;
    local->public.instances = instance;
    return &local->public;
}

void
flt_local_free(struct flt *flt, struct flt_local *plocal)
{
    struct flt_local_priv  *local =
        cork_container_of(plocal, struct flt_local_priv, public);
    free(local);
}

void *
flt_local_get_current_shard(struct flt *flt, struct flt_local *plocal)
{
    struct flt_local_priv  *local =
        cork_container_of(plocal, struct flt_local_priv, public);
    char  *instances = (char *) local->public.instances;
    size_t  padded_instance_size = local->padded_instance_size;
    return instances + padded_instance_size * flt->index;
}

void *
flt_local_get_shard(struct flt *flt, struct flt_local *plocal,
                    unsigned int index)
{
    struct flt_local_priv  *local =
        cork_container_of(plocal, struct flt_local_priv, public);
    char  *instances = (char *) local;
    size_t  padded_instance_size = local->padded_instance_size;
    return instances + padded_instance_size * index;
}
