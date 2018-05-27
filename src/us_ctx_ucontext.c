
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ucontext.h>

#include "debug.h"
#include "us_ctx.h"

#ifdef USE_MAKECONTEXT

static __thread struct us_ctx *new_thread_ctx;
static __thread ucontext_t *thread_creation_ctx;

static void us_makecontext_thread_start(void)
{
    struct us_ctx *ctx = new_thread_ctx;

    swapcontext(&ctx->context, thread_creation_ctx);

    (ctx->entry_func) (ctx);
}

void us_make_ctx(struct us_ctx *ctx, struct us_ctx *ret_ctx, void (*entry) (struct us_ctx *), void *stack, size_t len)
{
    ucontext_t thread_create_ctx;

    ctx->entry_func = entry;
    ctx->context.uc_link = &ret_ctx->context;
    ctx->context.uc_stack.ss_sp = stack;
    ctx->context.uc_stack.ss_size = len;
    ctx->context.uc_stack.ss_flags = 0;

    getcontext(&ctx->context);
    makecontext(&ctx->context, us_makecontext_thread_start, 0);

    new_thread_ctx = ctx;
    thread_creation_ctx = &thread_create_ctx;

    swapcontext(&thread_create_ctx, &ctx->context);
}

void *us_get_stack(struct us_ctx *ctx)
{
    return ctx->context.uc_stack.ss_sp;
}

void us_swapctx(struct us_ctx *old, struct us_ctx *new)
{
    swapcontext(&old->context, &new->context);
}

#endif
