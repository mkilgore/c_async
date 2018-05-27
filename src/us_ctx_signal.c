
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "debug.h"
#include "us_ctx.h"

/*
 * This implements userland stack-switching in a platform generic way by abusing
 * the `sigaltstack()` function, which lets you set a custom stack to be used by
 * signal handlers.
 *
 * The basic idea:
 *
 * 1. Set the signal handler stack to the stack we want our userland thread to use
 * 2. Trigger a custom signal handler to run,
 * 3. In that signal handler, do a `setjmp` on a `jmp_buf` stored in a location we can access later
 * 4. Return from the signal handler normally, and restore all of the previous signal settings
 * 5. Do a `longjmp` to the `jmp_buf` that was set inside of the signal handler.
 *    This causes us to do a `longjmp` onto the stack we set at the beginning, meaning any code that
 *    runs from that point will be executing using our custom stack.
 * 6. When that code is finished running, you need to do a `longjmp` out of the signal handler
 *    rather then do a regular return.
 *
 * The above steps get a bit messier in the actual code as you need to do some fancy
 * signal blocking and unblocking to ensure that our code is the only signal handler that will
 * ever execute between when we set the new stack, and when we restore the old stack.
 *
 * This is fairly easy to do via masking all of the signals and then using `sigsuspend()`.
 */

#ifndef USE_MAKECONTEXT

static __thread struct us_ctx *new_thread_ctx;

static void us_sighand_thread_start(int signum)
{
    struct us_ctx *ctx = new_thread_ctx;

    if (setjmp(ctx->context)) {
        (ctx->entry_func) (ctx);
        longjmp(ctx->ret_ctx->context, 1);
    }

    /* Normal return from signal handler.
     *
     * the trick is that since we did a setjmp above, we can longjmp
     * back into the original signal handler with our custom stack. */
}

void us_make_ctx(struct us_ctx *ctx, struct us_ctx *ret_ctx, void (*entry) (struct us_ctx *), void *stack, size_t len)
{
    sigset_t old_set;
    stack_t old_ss;
    sigset_t blocked_set;
    struct sigaction old_sigact;
    struct sigaction sigact;
    memset(&sigact, 0, sizeof(sigact));

    memset(ctx, 0, sizeof(*ctx));

    ctx->entry_func = entry;

    ctx->stack.ss_sp = stack;
    ctx->stack.ss_size = len;
    ctx->stack.ss_flags = 0;

    new_thread_ctx = ctx;

    sigfillset(&blocked_set);

    sigprocmask(SIG_SETMASK, &blocked_set, &old_set);

    sigaltstack(&ctx->stack, &old_ss);

    sigact.sa_handler = us_sighand_thread_start;
    sigact.sa_mask = blocked_set;
    sigact.sa_flags = SA_ONSTACK;
    sigaction(SIGUSR1, &sigact, &old_sigact);

    /* Send ourselves a SIGUSR1 to create our thread context in the new stack.
     * All signals are blocked, so this will not be caught until our sigsuspend. */
    kill(getpid(), SIGUSR1);

    sigdelset(&blocked_set, SIGUSR1);
    sigsuspend(&blocked_set);

    sigaltstack(&old_ss, NULL);
    sigaction(SIGUSR1, &old_sigact, NULL);
    sigprocmask(SIG_SETMASK, &old_set, NULL);
}

void *us_get_stack(struct us_ctx *ctx)
{
    return ctx->stack.ss_sp;
}

void us_swapctx(struct us_ctx *old, struct us_ctx *new)
{
    if (!setjmp(old->context))
        longjmp(new->context, 1);
}

#endif
