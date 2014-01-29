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
    void  *ud;
    flt_local_free_f  *free_instance;
};

struct flt_local *
flt_local_new(struct flt *pflt, void *ud,
              flt_local_new_f *new_instance,
              flt_local_free_f *free_instance)
{
    size_t  i;
    struct flt_priv  *flt = cork_container_of(pflt, struct flt_priv, public);
    struct flt_local_priv  *local = cork_new(struct flt_local_priv);
    local->ud = ud;
    local->free_instance = free_instance;
    local->public.instances = cork_calloc(flt->public.count, sizeof(void *));
    for (i = 0; i < flt->public.count; i++) {
        local->public.instances[i] = new_instance(pflt, ud);
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
    for (i = 0; i < flt->public.count; i++) {
        local->free_instance(pflt, local->ud, local->public.instances[i]);
    }
    free(local->public.instances);
    free(local);
}
