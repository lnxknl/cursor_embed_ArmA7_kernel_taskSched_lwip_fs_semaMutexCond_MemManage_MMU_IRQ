#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// WebSocket帧类型
#define WS_CONTINUATION  0x0
#define WS_TEXT         0x1
#define WS_BINARY      0x2
#define WS_CLOSE       0x8
#define WS_PING        0x9
#define WS_PONG        0xA

// WebSocket状态码
#define WS_CLOSE_NORMAL            1000
#define WS_CLOSE_GOING_AWAY        1001
#define WS_CLOSE_PROTOCOL_ERROR    1002
#define WS_CLOSE_UNSUPPORTED       1003
#define WS_CLOSE_NO_STATUS         1005
#define WS_CLOSE_ABNORMAL          1006
#define WS_CLOSE_INVALID_PAYLOAD   1007
#define WS_CLOSE_POLICY_VIOLATION  1008
#define WS_CLOSE_MESSAGE_TOO_BIG   1009
#define WS_CLOSE_EXTENSION_MISSING 1010
#define WS_CLOSE_SERVER_ERROR      1011

// WebSocket帧头
typedef struct {
    uint8_t fin:1;
    uint8_t rsv1:1;
    uint8_t rsv2:1;
    uint8_t rsv3:1;
    uint8_t opcode:4;
    uint8_t mask:1;
    uint8_t payload_len:7;
    uint8_t masking_key[4];
    uint64_t payload_length;
} ws_frame_header_t;

// WebSocket连接结构
typedef struct ws_connection {
    int socket;
    bool is_server;
    char *host;
    char *path;
    char *protocol;
    void *user_data;
    
    // 回调函数
    void (*on_connect)(struct ws_connection *conn);
    void (*on_message)(struct ws_connection *conn, const char *msg, size_t len);
    void (*on_close)(struct ws_connection *conn, int status);
    void (*on_error)(struct ws_connection *conn, const char *error);
    
    // 内部状态
    bool handshake_completed;
    char *receive_buffer;
    size_t buffer_size;
    size_t buffer_used;
} ws_connection_t;

// WebSocket服务器结构
typedef struct {
    int socket;
    char *host;
    int port;
    void *user_data;
    
    // 回调函数
    void (*on_client_connect)(ws_connection_t *client);
    void (*on_client_message)(ws_connection_t *client, const char *msg, size_t len);
    void (*on_client_close)(ws_connection_t *client, int status);
    void (*on_error)(const char *error);
} ws_server_t;

// API函数声明
ws_server_t *ws_server_create(const char *host, int port);
void ws_server_destroy(ws_server_t *server);
int ws_server_start(ws_server_t *server);
void ws_server_stop(ws_server_t *server);

ws_connection_t *ws_connect(const char *url);
void ws_disconnect(ws_connection_t *conn);
int ws_send(ws_connection_t *conn, const char *data, size_t len, int type);
int ws_send_text(ws_connection_t *conn, const char *text);
int ws_send_binary(ws_connection_t *conn, const void *data, size_t len);
int ws_send_ping(ws_connection_t *conn);
int ws_send_pong(ws_connection_t *conn);
int ws_send_close(ws_connection_t *conn, int status);

#endif 