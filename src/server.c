#include "websocket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_CLIENTS 1000
#define BUFFER_SIZE 65536

typedef struct {
    ws_connection_t *clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t lock;
} client_manager_t;

static client_manager_t client_manager = {
    .client_count = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void add_client(ws_connection_t *client)
{
    pthread_mutex_lock(&client_manager.lock);
    if (client_manager.client_count < MAX_CLIENTS) {
        client_manager.clients[client_manager.client_count++] = client;
    }
    pthread_mutex_unlock(&client_manager.lock);
}

static void remove_client(ws_connection_t *client)
{
    pthread_mutex_lock(&client_manager.lock);
    for (int i = 0; i < client_manager.client_count; i++) {
        if (client_manager.clients[i] == client) {
            // 移动最后一个客户端到当前位置
            client_manager.clients[i] = 
                client_manager.clients[--client_manager.client_count];
            break;
        }
    }
    pthread_mutex_unlock(&client_manager.lock);
}

static void broadcast_message(ws_server_t *server, const char *message, size_t len)
{
    pthread_mutex_lock(&client_manager.lock);
    for (int i = 0; i < client_manager.client_count; i++) {
        ws_connection_t *client = client_manager.clients[i];
        ws_send(client, message, len, WS_TEXT);
    }
    pthread_mutex_unlock(&client_manager.lock);
}

static void handle_client_message(ws_connection_t *conn,
                                const uint8_t *payload, size_t len,
                                uint8_t opcode)
{
    switch (opcode) {
        case WS_TEXT:
        case WS_BINARY:
            if (conn->on_message) {
                conn->on_message(conn, (const char *)payload, len);
            }
            break;
            
        case WS_PING:
            ws_send_pong(conn);
            break;
            
        case WS_PONG:
            // 可以更新最后一次pong时间
            break;
            
        case WS_CLOSE:
            {
                uint16_t status = WS_CLOSE_NORMAL;
                if (len >= 2) {
                    memcpy(&status, payload, 2);
                    status = ntohs(status);
                }
                ws_send_close(conn, status);
                if (conn->on_close) {
                    conn->on_close(conn, status);
                }
            }
            break;
    }
}

static void *client_thread(void *arg)
{
    ws_connection_t *conn = (ws_connection_t *)arg;
    uint8_t buffer[BUFFER_SIZE];
    ssize_t bytes;

    // 处理WebSocket握手
    bytes = recv(conn->socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) goto cleanup;
    buffer[bytes] = '\0';

    if (ws_handle_handshake(conn, (char *)buffer) < 0) {
        goto cleanup;
    }

    if (conn->on_connect) {
        conn->on_connect(conn);
    }

    // 主消息循环
    while (1) {
        ws_frame_header_t header;
        bytes = recv(conn->socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        size_t pos = 0;
        while (pos < bytes) {
            int header_size = ws_parse_frame_header(buffer + pos,
                                                  bytes - pos,
                                                  &header);
            if (header_size < 0) break;

            pos += header_size;
            if (pos + header.payload_length > bytes) break;

            uint8_t *payload = buffer + pos;
            if (header.mask) {
                ws_mask_payload(payload, header.payload_length,
                              header.masking_key);
            }

            handle_client_message(conn, payload, header.payload_length,
                                header.opcode);
            pos += header.payload_length;
        }
    }

cleanup:
    remove_client(conn);
    close(conn->socket);
    free(conn);
    return NULL;
}

ws_server_t *ws_server_create(const char *host, int port)
{
    ws_server_t *server = calloc(1, sizeof(ws_server_t));
    if (!server) return NULL;

    server->host = strdup(host);
    server->port = port;
    return server;
}

int ws_server_start(ws_server_t *server)
{
    struct sockaddr_in addr;
    int opt = 1;

    server->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket < 0) goto error;

    setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server->socket);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->port);
    addr.sin_addr.s_addr = inet_addr(server->host);

    if (bind(server->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        goto error;
    }

    if (listen(server->socket, SOMAXCONN) < 0) {
        goto error;
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_sock = accept(server->socket,
                               (struct sockaddr *)&client_addr,
                               &addr_len);
        if (client_sock < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            goto error;
        }

        set_nonblocking(client_sock);

        ws_connection_t *conn = calloc(1, sizeof(ws_connection_t));
        if (!conn) {
            close(client_sock);
            continue;
        }

        conn->socket = client_sock;
        conn->is_server = true;
        conn->on_message = server->on_client_message;
        conn->on_close = server->on_client_close;

        add_client(conn);

        pthread_t thread;
        pthread_create(&thread, NULL, client_thread, conn);
        pthread_detach(thread);
    }

error:
    if (server->on_error) {
        server->on_error(strerror(errno));
    }
    return -1;
} 