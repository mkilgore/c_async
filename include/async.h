#ifndef INCLUDE_ASYNC_H
#define INCLUDE_ASYNC_H

#include <unistd.h>
#include <ucontext.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "list.h"

struct task_ctx {
    ucontext_t context;
    ucontext_t *sched;
    int fd;
    int events; /* Represents the event flags we're waiting for */
    struct sockaddr addr;
    socklen_t addrlen;

    list_node_t node;
    void (*func) (struct task_ctx *ctx);

    int complete;
};

void async_scheduler(int listenfd, void (*conn_handler) (struct task_ctx *ctx));

/* These suspend the context until 'count' bytes have been read or written */
ssize_t async_read_full(struct task_ctx *, void *buf, size_t count);
ssize_t async_write_full(struct task_ctx *, const void *buf, size_t count);

ssize_t async_read(struct task_ctx *, void *buf, size_t count);
ssize_t async_write(struct task_ctx *, const void *buf, size_t count);

#endif
