/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include "libcork/core.h"
#include "libcork/ds.h"

#include "fleet.h"
#include "fleet/task.h"


/*-----------------------------------------------------------------------
 * Context-local data
 */

/* To eliminate false sharing we want to make sure that each element of the
 * instances array is in a separate cache line.  This involves two steps: first,
 * we have to round up the size of each element so that it's a multiple of the
 * cache line size.  This is handled in the fleet.h public header file with all
 * of the calls to flt_round_to_cache_line.  Second, we have to make sure that
 * the start of the array is also rounded to a cache line.  malloc() doesn't
 * guarantee this, and if we get an unaligned array, then each element will span
 * a cache line boundary, and we'll definitely get some false sharing.
 *
 * To support this second step, we have two pointers for the array of elements.
 * unaligned_instances is the raw pointer that we get back from malloc().
 * public.instances is guaranteed to be aligned to a cache line.  This aligned
 * pointer is visible via the public API, and is how user code gets pointers to
 * the individual elements of the array.  We have to keep the unaligned pointer
 * around, as well, since we'll have to pass the same pointer to free() that we
 * got from malloc().
 */

struct flt_local_priv {
    struct flt_local  public;
    void  *unaligned_instances;
    size_t  padded_size;
    void  *ud;
    flt_local_done_f  *done_instance;
};

static void *
align_to_cache_line(void *unaligned)
{
    uintptr_t  addr = (uintptr_t) unaligned;
    if ((addr % FLT_CACHE_LINE_SIZE) != 0) {
        addr = ((addr / FLT_CACHE_LINE_SIZE) + 1) * FLT_CACHE_LINE_SIZE;
        return (void *) addr;
    } else {
        return unaligned;
    }
}

struct flt_local *
flt_local_new_size(struct flt *pflt, size_t instance_size, void *ud,
                   flt_local_init_f *init_instance,
                   flt_local_done_f *done_instance)
{
    unsigned int  i;
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_local_priv  *local = cork_new(struct flt_local_priv);
    size_t  padded_size = flt_round_to_cache_line(instance_size);
    size_t  full_size;
    char  *instance;

    local->ud = ud;
    local->done_instance = done_instance;
    local->padded_size = padded_size;

    /* Normally the array is simply `count` copies of padded size.  But since we
     * might need to align the pointer after it's been allocated, we allocate an
     * extra cache line of space.  This gives us the wiggle room that we need to
     * bump the pointer up to the next cache line boundary without running out
     * of space at the end of the array. */
    full_size = flt->public.count * padded_size;
    local->unaligned_instances =
        cork_calloc(1, full_size + FLT_CACHE_LINE_SIZE);
    instance = align_to_cache_line(local->unaligned_instances);
    local->public.instances = instance;

    /* Now that we have an aligned array of elements, initialize each one. */
    for (i = 0; i < flt->public.count; i++, instance += padded_size) {
        init_instance(pflt, ud, instance);
    }
    return &local->public;
}

void
flt_local_free(struct flt *pflt, struct flt_local *plocal)
{
    size_t  i;
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_local_priv  *local =
        cork_container_of(plocal, struct flt_local_priv, public);
    char  *instance;
    for (i = 0, instance = local->public.instances; i < flt->public.count;
         i++, instance += local->padded_size) {
        local->done_instance(pflt, local->ud, instance);
    }
    free(local->unaligned_instances);
    free(local);
}
