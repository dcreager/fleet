/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>

#include "fleet.h"
#include "examples.h"


static unsigned long  min;
static unsigned long  max;
static unsigned long  batch_size;
static unsigned long  result;

static void
configure(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: concurrent_batched [batch size] [count]\n");
        exit(EXIT_FAILURE);
    }
    min = 0;
    max = flt_parse_ulong(argv[1]);
    batch_size = flt_parse_ulong(argv[0]);
}

static void
print_name(FILE *out)
{
    fprintf(out, "concurrent_batched:%lu:%lu", batch_size, max);
}

static void
run_native(void)
{
    unsigned long  sum = 0;
    unsigned long  i;
    for (i = min; i < max; i++) {
        sum += i;
    }
    result = sum;
}

static flt_task_f  add_one;
static flt_task_f  merge_batches;
static flt_task_f  schedule_one;
static flt_task_f  schedule;

struct add_ctx {
    struct flt_scounter  *counter;
    unsigned long  result;
};

static void *
add_ctx__migrate(struct flt *from, struct flt *to, void *ud, size_t i,
                 void *migrate_ud)
{
    struct add_ctx  *from_ctx = ud;
    flt_scounter_migrate(from, to, from_ctx->counter);
    return flt_local_migrate(from, to, from_ctx);
}


static void
merge_one_batch(struct flt *flt, unsigned int index, struct add_ctx *ctx,
                int dummy)
{
    result += ctx->result;
}

static void
merge_batches(struct flt *flt, void *ud, size_t i)
{
    struct add_ctx  *ctx = ud;
    flt_local_visit(flt, struct add_ctx, ctx, merge_one_batch, 0);
    flt_local_free(flt, ctx);
}

static void
add_one(struct flt *flt, void *ud, size_t index)
{
    struct add_ctx  *ctx = ud;
    unsigned long  i = index;
    ctx->result += i;
    if (flt_scounter_dec(flt, ctx->counter)) {
        flt_run(flt, merge_batches, ctx, 0);
    }
}

static void
schedule_one(struct flt *flt, void *ud, size_t index)
{
    struct add_ctx  *ctx = ud;
    unsigned long  i = index;
    unsigned long  j = i + batch_size;

    if (j > max) {
        j = max;
    } else {
        flt_scounter_inc(flt, ctx->counter);
        flt_run_migratable(flt, schedule_one, ctx, j, add_ctx__migrate, NULL);
    }

    for (; i < j; i++) {
        flt_scounter_inc(flt, ctx->counter);
        flt_run_migratable(flt, add_one, ctx, i, add_ctx__migrate, NULL);
    }

    if (flt_scounter_dec(flt, ctx->counter)) {
        flt_run(flt, merge_batches, ctx, 0);
    }
}

static void
ctx_init(struct flt *flt, void *ud, void *vinstance, unsigned int index)
{
    struct flt_scounter  *counter = ud;
    struct add_ctx  *ctx = vinstance;
    ctx->counter = flt_scounter_get(flt, counter, index);
    ctx->result = 0;
}

static void
ctx_done(struct flt *flt, void *ud, void *vinstance, unsigned int index)
{
}

static void
schedule(struct flt *flt, void *ud, size_t min)
{
    struct flt_scounter  *counter;
    struct add_ctx  *ctx;
    counter = flt_scounter_new(flt);
    ctx = flt_local_new(flt, struct add_ctx, counter, ctx_init, ctx_done);
    flt_scounter_inc(flt, ctx->counter);
    flt_run_migratable(flt, schedule_one, ctx, min, add_ctx__migrate, NULL);
}

static void
run_in_fleet(struct flt_fleet *fleet)
{
    result = 0;
    flt_fleet_run(fleet, schedule, NULL, min);
}

static int
verify(void)
{
    unsigned long  expected = max / 2 * (max - 1);
    flt_check_result(concurrent_batched, "%lu", result, expected);
    return 0;
}

struct flt_example  concurrent_batched = {
    configure,
    print_name,
    run_native,
    run_in_fleet,
    verify
};
