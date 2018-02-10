
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "async.h"

void con_handler(struct task_ctx *ctx)
{
    printf("in con_handler\n");
    while (1) {
        char buf[10];
        ssize_t count = async_read(ctx, buf, sizeof(buf));

        if (!count)
            break;

        printf("Read %d: %.*s\n", ctx->fd, (int)count, buf);

        async_write_full(ctx, buf, count);
    }
}

int main(int argc, char **argv)
{
    int server = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in inaddr;

    inaddr.sin_family = AF_INET;
    inaddr.sin_port = htons(12345);
    inaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(server, (struct sockaddr *)&inaddr, sizeof(inaddr));

    listen(server, 10);
    async_scheduler(server, con_handler);

    return 0;
}

