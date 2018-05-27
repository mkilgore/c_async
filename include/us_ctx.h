#ifndef INCLUDE_US_CTX_H
#define INCLUDE_US_CTX_H

#include "config.h"

#ifdef USE_MAKECONTEXT
#include <ucontext.h>

struct us_ctx {
    ucontext_t context;
    void (*entry_func) (struct us_ctx *);
};

#else

#include <signal.h>
#include <setjmp.h>

/* use signal handler hack */
struct us_ctx {
    jmp_buf context;
    stack_t stack;

    struct us_ctx *ret_ctx;
    void (*entry_func) (struct us_ctx *);
};

#endif

void us_make_ctx(struct us_ctx *ctx, struct us_ctx *ret_ctx, void (*entry) (struct us_ctx *), void *stack, size_t len);
void *us_get_stack(struct us_ctx *ctx);
void us_swapctx(struct us_ctx *old, struct us_ctx *new);

#endif
