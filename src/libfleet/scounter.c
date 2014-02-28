/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include "libcork/core.h"

#include "fleet.h"
#include "fleet/threads.h"


/*-----------------------------------------------------------------------
 * Semaphore counters
 */

struct flt_scounter_global {
    struct flt_counter  active_count;
};

struct flt_scounter {
    struct flt_scounter_global  *global;
    size_t  count;
};

static void
flt_scounter_init(struct flt *flt, void *ud, void *vinstance,
                  unsigned int index)
{
    struct flt_scounter_global  *global = ud;
    struct flt_scounter  *counter = vinstance;
    counter->global = global;
    counter->count = 0;
}

static void
flt_scounter_done(struct flt *flt, void *ud, void *vinstance,
                  unsigned int index)
{
}

struct flt_scounter *
flt_scounter_new(struct flt *flt)
{
    struct flt_scounter  *counter;
    struct flt_scounter_global  *global = cork_new(struct flt_scounter_global);
    counter = flt_local_new
        (flt, struct flt_scounter, global,
         flt_scounter_init, flt_scounter_done);
    flt_counter_init(&global->active_count);
    return counter;
}

void
flt_scounter_free(struct flt *flt, struct flt_scounter *counter)
{
    struct flt_scounter_global  *global = counter->global;
    flt_local_free(flt, counter);
    free(global);
}

struct flt_scounter *
flt_scounter_get(struct flt *flt, struct flt_scounter *counter,
                 unsigned int index)
{
    return flt_local_get(flt, counter, index);
}

void
flt_scounter_inc(struct flt *flt, struct flt_scounter *counter)
{
    if (counter->count++ == 0) {
        flt_counter_inc(&counter->global->active_count);
    }
}

bool
flt_scounter_dec(struct flt *flt, struct flt_scounter *counter)
{
    if (--counter->count == 0) {
        if (flt_counter_dec(&counter->global->active_count)) {
            flt_scounter_free(flt, counter);
            return true;
        }
    }
    return false;
}

struct flt_scounter *
flt_scounter_migrate(struct flt *from, struct flt *to,
                     struct flt_scounter *from_counter)
{
    struct flt_scounter_global  *global = from_counter->global;
    struct flt_scounter  *to_counter =
        flt_local_migrate(from, to, from_counter);

    from_counter->count--;
    to_counter->count++;

    if (to_counter->count == 1) {
        if (from_counter->count == 0) {
            /* The from context just became inactive and the to context just
             * became active with this migration.  The net result is that the
             * total number of active contexts didn't change, so we don't need
             * to bump the global counter. */
        } else {
            flt_counter_inc(&global->active_count);
        }
    } else if (from_counter->count == 0) {
        (void) flt_counter_dec(&global->active_count);
    }

    return to_counter;
}
