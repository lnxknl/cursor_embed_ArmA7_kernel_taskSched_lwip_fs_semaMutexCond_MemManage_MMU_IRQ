#include "websocket.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define MAX_HEADERS 100
#define BUFFER_SIZE 8192

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
} http_request_t;

static char *base64_encode(const unsigned char *input, int length)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    char *buffer;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    buffer = malloc(bptr->length);
    memcpy(buffer, bptr->data, bptr->length - 1);
    buffer[bptr->length - 1] = 0;

    BIO_free_all(b64);
    return buffer;
}

static void generate_websocket_key(const char *client_key, char *accept_key)
{
    unsigned char sha1_data[SHA_DIGEST_LENGTH];
    char concat_key[256];
    
    // 连接客户端key和GUID
    snprintf(concat_key, sizeof(concat_key), "%s%s", client_key, WS_GUID);
    
    // 计算SHA1
    SHA1((unsigned char *)concat_key, strlen(concat_key), sha1_data);
    
    // Base64编码
    char *encoded = base64_encode(sha1_data, SHA_DIGEST_LENGTH);
    strcpy(accept_key, encoded);
    free(encoded);
}

static int parse_http_request(const char *buffer, http_request_t *request)
{
    char *line = strtok((char *)buffer, "\r\n");
    if (!line) return -1;

    // 解析请求行
    sscanf(line, "%s %s %s", request->method, request->path, request->version);

    // 解析请求头
    request->header_count = 0;
    while ((line = strtok(NULL, "\r\n")) && *line) {
        char *colon = strchr(line, ':');
        if (colon && request->header_count < MAX_HEADERS) {
            *colon = '\0';
            http_header_t *header = &request->headers[request->header_count++];
            header->name = line;
            header->value = colon + 2;  // Skip ": "
        }
    }

    return 0;
}

static const char *get_header_value(const http_request_t *request, const char *name)
{
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i].name, name) == 0) {
            return request->headers[i].value;
        }
    }
    return NULL;
}

int ws_handle_handshake(ws_connection_t *conn, const char *request_buffer)
{
    http_request_t request;
    char response[BUFFER_SIZE];
    char accept_key[64];

    // 解析HTTP请求
    if (parse_http_request(request_buffer, &request) < 0) {
        return -1;
    }

    // 验证WebSocket请求
    if (strcmp(request.method, "GET") != 0) return -1;
    
    const char *upgrade = get_header_value(&request, "Upgrade");
    if (!upgrade || strcasecmp(upgrade, "websocket") != 0) return -1;
    
    const char *connection = get_header_value(&request, "Connection");
    if (!connection || strstr(connection, "Upgrade") == NULL) return -1;
    
    const char *ws_key = get_header_value(&request, "Sec-WebSocket-Key");
    if (!ws_key) return -1;
    
    const char *ws_version = get_header_value(&request, "Sec-WebSocket-Version");
    if (!ws_version || strcmp(ws_version, "13") != 0) return -1;

    // 生成WebSocket Accept Key
    generate_websocket_key(ws_key, accept_key);

    // 构建响应
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    // 发送响应
    if (send(conn->socket, response, response_len, 0) != response_len) {
        return -1;
    }

    conn->handshake_completed = true;
    return 0;
} 