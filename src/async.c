
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "debug.h"
#include "list.h"
#include "async.h"

static int task_count = 0;
static list_head_t task_list = LIST_HEAD_INIT(task_list);

ssize_t async_read_full(struct task_ctx *ctx, void *buf, size_t count)
{
    size_t cur_count = 0;

    while (1) {
        ssize_t total = read(ctx->fd, buf + cur_count, count - cur_count);
        dbg_printf("--- ASYNC READ %d - %zd BYTES ---\n", ctx->fd, total);

        cur_count += total;

        /* A read of zero indicates EOF */
        if (total == 0)
            break;

        if (cur_count < count) {
            ctx->events = POLLIN;
            us_swapctx(&ctx->context, ctx->sched);
        }
    }

    return cur_count;
}

ssize_t async_read(struct task_ctx *ctx, void *buf, size_t count)
{
    ssize_t total = 0;

    while (1) {
        total = read(ctx->fd, buf, count);
        dbg_printf("--- ASYNC READ %d - %zd BYTES ---\n", ctx->fd, total);

        if (total != -1)
            break;

        ctx->events = POLLIN;
        us_swapctx(&ctx->context, ctx->sched);
    }

    return total;
}


ssize_t async_write_full(struct task_ctx *ctx, const void *buf, size_t count)
{
    size_t cur_count = 0;

    while (1) {
        size_t total = write(ctx->fd, buf + cur_count, count - cur_count);
        dbg_printf("--- ASYNC WRITE %d - %zd BYTES ---\n", ctx->fd, total);

        cur_count += total;

        if (cur_count == count)
            break;

        if (cur_count < count) {
            ctx->events = POLLOUT;
            us_swapctx(&ctx->context, ctx->sched);
        }
    }

    return cur_count;
}

ssize_t async_write(struct task_ctx *ctx, const void *buf, size_t count)
{
    ssize_t total = 0;

    while (1) {
        total = write(ctx->fd, buf, count);
        dbg_printf("--- ASYNC WRITE %d - %zd BYTES ---\n", ctx->fd, total);

        if (total != -1)
            break;

        ctx->events = POLLOUT;
        us_swapctx(&ctx->context, ctx->sched);
    }

    return total;
}

static void apply_nonblock(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static void async_thread_start(struct us_ctx *us_ctx)
{
    struct task_ctx *ctx = container_of(us_ctx, struct task_ctx, context);

    dbg_printf("--- STARTING ASYNC TASK %d ---\n", ctx->fd);
    (ctx->func) (ctx);
    dbg_printf("--- ENDING ASYNC TASK %d ---\n", ctx->fd);
    ctx->complete = 1;
}

static struct task_ctx *create_async_thread(struct us_ctx *sched)
{
    struct task_ctx *ctx = malloc(sizeof(*ctx));
    size_t stack_size = 4096 * 20;
    void *stack = malloc(stack_size);

    memset(ctx, 0, sizeof(*ctx));

    us_make_ctx(&ctx->context, sched, async_thread_start, stack, stack_size);

    return ctx;
}

static void free_async_thread(struct task_ctx *ctx)
{
    free(us_get_stack(&ctx->context));
    free(ctx);
}

void async_scheduler(int listenfd, void (*conn_handler) (struct task_ctx *ctx))
{
    struct us_ctx scheduler_context;
    struct pollfd *fds = NULL;

    apply_nonblock(listenfd);

    while (1) {
        int i = 0;
        struct task_ctx *ctx;
        size_t fds_count = 1 + task_count;
        fds = realloc(fds, sizeof(*fds) * fds_count);
        memset(fds, 0, sizeof(*fds) * fds_count);

        fds[0].fd = listenfd;
        fds[0].events = POLLIN;

        list_foreach_entry(&task_list, ctx, node) {
            i++;
            fds[i].fd = ctx->fd;
            fds[i].events = ctx->events;
        }

        dbg_printf("--- POLLING ---\n");
        poll(fds, fds_count, -1);

        i = 0;
        list_foreach_entry(&task_list, ctx, node) {
            i++;

            if (fds[i].revents & ctx->events) {
                dbg_printf("--- SWAPPING TO CTX %d ---\n", ctx->fd);
                us_swapctx(&scheduler_context, &ctx->context);
            }
        }

        /* Clean-up tasks that have completed */
        struct task_ctx *ctx2;
        for (ctx = list_first_entry(&task_list, struct task_ctx, node);
             &ctx->node != &task_list;
             ctx = ctx2) {
            ctx2 = list_next_entry(ctx, node);

            if (ctx->complete) {
                dbg_printf("--- CLEANING UP %d ---\n", ctx->fd);
                list_del(&ctx->node);
                close(ctx->fd);
                free_async_thread(ctx);
            }
        }

        /* New connection, create new task */
        if (fds[0].revents & POLLIN) {
            struct sockaddr addr;
            socklen_t addrlen = sizeof(addr);

            int new_fd = accept(listenfd, &addr, &addrlen);
            apply_nonblock(new_fd);

            struct task_ctx *new_task = create_async_thread(&scheduler_context);

            new_task->sched = &scheduler_context;
            new_task->fd = new_fd;
            new_task->addrlen = addrlen;
            memcpy(&new_task->addr, &addr, sizeof(addr));
            new_task->func = conn_handler;
            new_task->events = POLLIN | POLLOUT;

            list_add_tail(&task_list, &new_task->node);
            task_count++;
            dbg_printf("--- ADDED TASK %d ---\n", new_fd);
        }
    }

    free(fds);
}

