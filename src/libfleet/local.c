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

struct flt_local_priv {
    struct flt_local  public;
    size_t  padded_size;
    void  *ud;
    flt_local_done_f  *done_instance;
};

struct flt_local *
flt_local_new_size(struct flt *pflt, size_t instance_size, void *ud,
                   flt_local_init_f *init_instance,
                   flt_local_done_f *done_instance)
{
    size_t  i;
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_local_priv  *local = cork_new(struct flt_local_priv);
    size_t  padded_size = flt_round_to_cache_line(instance_size);
    char  *instance;
    local->ud = ud;
    local->done_instance = done_instance;
    local->padded_size = padded_size;
    instance = cork_calloc(flt->public.count, padded_size);
    local->public.instances = instance;
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
    free(local->public.instances);
    free(local);
}
