
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "async.h"
#include "picohttpparser.h"
#include "arg_parser.h"

static const char *c_async_version = "c_async";
static const char *arg_str = "[Flags]";

#define XARGS \
    X(port,    "port",     'p',  1, "Port Number", "Default: 2222") \
    X(help,    "help",     'h',  0, NULL,          "Display help") \
    X(version, "version",  'v',  0, NULL,          "Display version information") \
    X(last,    NULL,       '\0', 0, NULL,          NULL)

enum arg_index {
    ARG_EXTRA = ARG_PARSER_EXTRA,
    ARG_ERR = ARG_PARSER_ERR,
    ARG_DONE = ARG_PARSER_DONE,
#define X(enu, id, op, arg, arg_text, help_text) ARG_ENUM(enu)
    XARGS
#undef X
};

static const struct arg c_async_args[] = {
#define X(enu, id, op, arg, arg_text, help_text) CREATE_ARG(enu, id, op, arg, arg_text, help_text)
    XARGS
#undef X
};

void handle_header(struct task_ctx *ctx, char *header, size_t len)
{
    const char *method;
    size_t method_len;
    const char *path;
    size_t path_len;
    int minor_version;
    struct phr_header headers[20];
    size_t num_headers = sizeof(headers) / sizeof(*headers);

    phr_parse_request(header, len, &method, &method_len, &path, &path_len, &minor_version, headers, &num_headers, 0);

    printf("HTTP method: %.*s\n", (int)method_len, method);
    printf("HTTP path: %.*s\n", (int)path_len, path);
    printf("HTTP version: %d\n", minor_version);

    int i;
    for (i = 0; i < num_headers; i++) {
        printf("HTTP header: %.*s\n", (int)headers[i].name_len, headers[i].name);
        printf("HTTP value: %.*s\n", (int)headers[i].value_len, headers[i].value);
    }

    char html_response[4096];

    snprintf(html_response, sizeof(html_response),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<body>\n"
        "  <p>Hello, World!</p>\n"
        "  <p>Path: %.*s</p>\n"
        "</body>\n"
        "</html>\n",
        (int)path_len, path);

    char http_response[8192];

    snprintf(http_response, sizeof(http_response),
        "HTTP/1.1 200 OK\n"
        "Content-Type: text/html\n"
        "Content-Length: %zd\n"
        "\n"
        "%s", strlen(html_response), html_response);

    async_write_full(ctx, http_response, strlen(http_response));
}

void http_handler(struct task_ctx *ctx)
{
    size_t capacity = 4096;
    size_t length = 0;
    char *header = malloc(capacity);

    printf("Entered HTTP handler\n");

    while (1) {
        ssize_t count = async_read(ctx, header + length, capacity - length);

        if (count == 0)
            break;

        length += count;

        /* FIXME: This only works for GET. Methods like POST can have data after the two newlines */
        if (header[length - 1] == '\n' && header[length - 2] == '\r'
            && header[length - 3] == '\n' && header[length - 4] == '\r')
            break;

        if (capacity - length < 1024) {
            capacity += 4096;
            header = realloc(header, capacity);
        }
    }

    handle_header(ctx, header, length);
    free(header);
}

int main(int argc, char **argv)
{
    int server;
    int portno = 2222;
    struct sockaddr_in inaddr;

    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, c_async_args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, "", "", c_async_args);
            return 0;
        case ARG_version:
            printf("%s\n", c_async_version);
            return 0;

        case ARG_port:
            portno = atoi(argarg);
            break;

        case ARG_EXTRA:
        case ARG_ERR:
        case ARG_last:
            printf("%s: Unexpected Argument '%s'\n", argv[0], argarg);
            return 0;

        case ARG_DONE:
            break;
        }
    }

    server = socket(AF_INET, SOCK_STREAM, 0);
    inaddr.sin_family = AF_INET;
    inaddr.sin_port = htons(portno);
    inaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(server, (struct sockaddr *)&inaddr, sizeof(inaddr));

    listen(server, 100);
    async_scheduler(server, http_handler);

    return 0;
}

