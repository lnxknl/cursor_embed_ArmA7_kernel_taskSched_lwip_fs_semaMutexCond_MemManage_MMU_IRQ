#include "epoll.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 4096
#define MAX_HEADERS 100
#define SERVER_PORT 8080

typedef struct {
    char *name;
    char *value;
} http_header_t;

typedef struct {
    char method[16];
    char path[1024];
    char version[16];
    http_header_t headers[MAX_HEADERS];
    int header_count;
    char *body;
    size_t body_length;
} http_request_t;

typedef struct {
    int fd;
    char buffer[BUFFER_SIZE];
    size_t buffer_pos;
    http_request_t request;
} client_context_t;

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void send_response(client_context_t *ctx, int status_code,
                         const char *status_text, const char *body)
{
    char response[BUFFER_SIZE];
    int len = snprintf(response, sizeof(response),
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "%s",
                      status_code, status_text,
                      strlen(body), body);
    
    write(ctx->fd, response, len);
}

static void handle_request(client_context_t *ctx)
{
    // 解析请求行
    char *line = strtok(ctx->buffer, "\r\n");
    if (!line) {
        send_response(ctx, 400, "Bad Request", "Invalid request line");
        return;
    }

    sscanf(line, "%s %s %s", ctx->request.method,
           ctx->request.path, ctx->request.version);

    // 解析请求头
    ctx->request.header_count = 0;
    while ((line = strtok(NULL, "\r\n")) && *line) {
        char *colon = strchr(line, ':');
        if (colon && ctx->request.header_count < MAX_HEADERS) {
            *colon = '\0';
            http_header_t *header = &ctx->request.headers[ctx->request.header_count++];
            header->name = line;
            header->value = colon + 2;  // Skip ": "
        }
    }

    // 处理GET请求
    if (strcmp(ctx->request.method, "GET") == 0) {
        if (strcmp(ctx->request.path, "/") == 0) {
            const char *body =
                "<html>"
                "<head><title>Welcome</title></head>"
                "<body>"
                "<h1>Welcome to High Performance HTTP Server</h1>"
                "<p>This server is powered by custom epoll implementation.</p>"
                "</body>"
                "</html>";
            send_response(ctx, 200, "OK", body);
        } else {
            send_response(ctx, 404, "Not Found", "Page not found");
        }
    } else {
        send_response(ctx, 405, "Method Not Allowed", "Method not supported");
    }
}

static void accept_connection(int epfd, int server_fd)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept failed");
        return;
    }

    // 设置非阻塞
    set_nonblocking(client_fd);

    // 创建客户端上下文
    client_context_t *ctx = calloc(1, sizeof(client_context_t));
    ctx->fd = client_fd;

    // 添加到epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // 边缘触发模式
    ev.data.ptr = ctx;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        perror("epoll_ctl failed");
        close(client_fd);
        free(ctx);
        return;
    }

    printf("New connection from %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));
}

static void handle_client(int epfd, client_context_t *ctx, uint32_t events)
{
    if (events & EPOLLIN) {
        // 读取请求
        ssize_t n = read(ctx->fd, ctx->buffer + ctx->buffer_pos,
                        BUFFER_SIZE - ctx->buffer_pos - 1);
        
        if (n <= 0) {
            if (n < 0 && errno != EAGAIN) {
                perror("read failed");
            }
            goto cleanup;
        }

        ctx->buffer_pos += n;
        ctx->buffer[ctx->buffer_pos] = '\0';

        // 检查是否收到完整的HTTP请求
        if (strstr(ctx->buffer, "\r\n\r\n")) {
            handle_request(ctx);
            goto cleanup;
        }

        return;
    }

cleanup:
    epoll_ctl(epfd, EPOLL_CTL_DEL, ctx->fd, NULL);
    close(ctx->fd);
    free(ctx);
}

int main(void)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    // 允许地址重用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(SERVER_PORT)
    };

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        return 1;
    }

    // 监听连接
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen failed");
        return 1;
    }

    // 创建epoll实例
    int epfd = epoll_create(1);
    if (epfd < 0) {
        perror("epoll_create failed");
        return 1;
    }

    // 添加服务器socket到epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl failed");
        return 1;
    }

    printf("HTTP server listening on port %d...\n", SERVER_PORT);

    // 事件循环
    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                accept_connection(epfd, server_fd);
            } else {
                handle_client(epfd, events[i].data.ptr, events[i].events);
            }
        }
    }

    close(server_fd);
    epoll_close(epfd);
    return 0;
} 