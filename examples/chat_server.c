#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

#define MAX_MESSAGE_SIZE 1024

typedef struct {
    char username[32];
    bool authenticated;
} client_data_t;

// 消息处理
static void handle_message(ws_connection_t *conn, const char *message, size_t len)
{
    client_data_t *client = (client_data_t *)conn->user_data;
    json_object *root = json_tokener_parse(message);
    
    if (!root) {
        const char *error = "{\"type\":\"error\",\"message\":\"Invalid JSON\"}";
        ws_send_text(conn, error);
        return;
    }

    json_object *type_obj;
    if (!json_object_object_get_ex(root, "type", &type_obj)) {
        const char *error = "{\"type\":\"error\",\"message\":\"Missing type\"}";
        ws_send_text(conn, error);
        json_object_put(root);
        return;
    }

    const char *type = json_object_get_string(type_obj);

    if (strcmp(type, "auth") == 0) {
        // 处理认证
        json_object *username_obj;
        if (json_object_object_get_ex(root, "username", &username_obj)) {
            const char *username = json_object_get_string(username_obj);
            strncpy(client->username, username, sizeof(client->username) - 1);
            client->authenticated = true;

            // 发送欢迎消息
            char welcome[256];
            snprintf(welcome, sizeof(welcome),
                    "{\"type\":\"system\",\"message\":\"Welcome, %s!\"}",
                    client->username);
            ws_send_text(conn, welcome);

            // 广播新用户加入
            char broadcast[256];
            snprintf(broadcast, sizeof(broadcast),
                    "{\"type\":\"system\",\"message\":\"%s has joined\"}",
                    client->username);
            // TODO: 广播给其他用户
        }
    }
    else if (strcmp(type, "message") == 0) {
        // 处理聊天消息
        if (!client->authenticated) {
            const char *error = "{\"type\":\"error\","
                              "\"message\":\"Please authenticate first\"}";
            ws_send_text(conn, error);
        } else {
            json_object *content_obj;
            if (json_object_object_get_ex(root, "content", &content_obj)) {
                const char *content = json_object_get_string(content_obj);
                
                // 创建广播消息
                json_object *broadcast = json_object_new_object();
                json_object_object_add(broadcast, "type",
                    json_object_new_string("message"));
                json_object_object_add(broadcast, "username",
                    json_object_new_string(client->username));
                json_object_object_add(broadcast, "content",
                    json_object_new_string(content));
                
                const char *broadcast_str = json_object_to_json_string(broadcast);
                // TODO: 广播给所有用户
                
                json_object_put(broadcast);
            }
        }
    }

    json_object_put(root);
}

// 客户端连接处理
static void on_client_connect(ws_connection_t *conn)
{
    client_data_t *client = calloc(1, sizeof(client_data_t));
    conn->user_data = client;
    
    printf("New client connected\n");
}

// 客户端断开处理
static void on_client_close(ws_connection_t *conn, int status)
{
    client_data_t *client = (client_data_t *)conn->user_data;
    
    if (client->authenticated) {
        // 广播用户离开
        char broadcast[256];
        snprintf(broadcast, sizeof(broadcast),
                "{\"type\":\"system\",\"message\":\"%s has left\"}",
                client->username);
        // TODO: 广播给其他用户
    }
    
    free(client);
    printf("Client disconnected (status: %d)\n", status);
}

// 错误处理
static void on_error(const char *error)
{
    printf("Error: %s\n", error);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    ws_server_t *server = ws_server_create(argv[1], atoi(argv[2]));
    if (!server) {
        printf("Failed to create server\n");
        return 1;
    }

    // 设置回调函数
    server->on_client_connect = on_client_connect;
    server->on_client_message = handle_message;
    server->on_client_close = on_client_close;
    server->on_error = on_error;

    printf("Chat server starting on %s:%s...\n", argv[1], argv[2]);
    
    // 启动服务器
    if (ws_server_start(server) < 0) {
        printf("Failed to start server\n");
        ws_server_destroy(server);
        return 1;
    }

    return 0;
} 