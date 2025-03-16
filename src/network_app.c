#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include <string.h>

// HTTP服务器配置
#define HTTP_SERVER_PORT    80
#define MAX_HTTP_CLIENT    5
#define HTTP_BUFFER_SIZE   2048

// HTTP响应头
const char http_html_header[] = "HTTP/1.1 200 OK\r\n"
    "Content-type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n";

// HTTP页面内容
const char http_html_body[] = "<!DOCTYPE html><html><body>"
    "<h1>Welcome to lwIP Web Server</h1>"
    "<p>This is a simple web server using lwIP stack.</p>"
    "</body></html>";

// HTTP服务器任务
static void http_server_thread(void *arg)
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char *recv_buffer;
    int recv_len;

    // 创建服务器socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        printf("Cannot create socket\n");
        return;
    }

    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(HTTP_SERVER_PORT);

    // 绑定地址
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Cannot bind socket\n");
        close(server_sock);
        return;
    }

    // 监听连接
    if (listen(server_sock, MAX_HTTP_CLIENT) < 0) {
        printf("Cannot listen on socket\n");
        close(server_sock);
        return;
    }

    printf("HTTP server started on port %d\n", HTTP_SERVER_PORT);

    // 分配接收缓冲区
    recv_buffer = mem_malloc(HTTP_BUFFER_SIZE);
    if (recv_buffer == NULL) {
        close(server_sock);
        return;
    }

    while (1) {
        // 等待客户端连接
        client_len = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock < 0) {
            continue;
        }

        printf("New client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 接收HTTP请求
        recv_len = recv(client_sock, recv_buffer, HTTP_BUFFER_SIZE - 1, 0);
        if (recv_len > 0) {
            recv_buffer[recv_len] = '\0';
            printf("Received request:\n%s\n", recv_buffer);

            // 发送HTTP响应
            send(client_sock, http_html_header, strlen(http_html_header), 0);
            send(client_sock, http_html_body, strlen(http_html_body), 0);
        }

        // 关闭连接
        close(client_sock);
    }

    // 释放资源
    mem_free(recv_buffer);
    close(server_sock);
}

// MQTT客户端配置
#define MQTT_BROKER_IP    "192.168.1.100"
#define MQTT_BROKER_PORT  1883
#define MQTT_CLIENT_ID    "lwip_mqtt_client"
#define MQTT_USERNAME     "user"
#define MQTT_PASSWORD     "password"

// MQTT客户端任务
static void mqtt_client_thread(void *arg)
{
    struct netconn *conn;
    err_t err;
    
    while (1) {
        // 创建TCP连接
        conn = netconn_new(NETCONN_TCP);
        if (conn == NULL) {
            printf("Cannot create MQTT connection\n");
            continue;
        }

        // 连接到MQTT代理
        err = netconn_connect(conn, IP_ADDR_MQTT_BROKER, MQTT_BROKER_PORT);
        if (err != ERR_OK) {
            printf("Cannot connect to MQTT broker\n");
            netconn_delete(conn);
            sys_msleep(5000);
            continue;
        }

        printf("Connected to MQTT broker\n");

        // 发送MQTT CONNECT包
        mqtt_connect(conn, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);

        // 订阅主题
        mqtt_subscribe(conn, "sensor/temperature", 0);
        mqtt_subscribe(conn, "sensor/humidity", 0);

        // 处理MQTT消息
        while (1) {
            struct netbuf *buf;
            void *data;
            u16_t len;

            // 接收数据
            err = netconn_recv(conn, &buf);
            if (err != ERR_OK) {
                break;
            }

            netbuf_data(buf, &data, &len);
            mqtt_handle_message(data, len);
            netbuf_delete(buf);
        }

        // 断开连接
        netconn_close(conn);
        netconn_delete(conn);
        printf("Disconnected from MQTT broker\n");

        // 等待重连
        sys_msleep(5000);
    }
}

// 网络初始化
void network_init(void)
{
    struct ip_addr ipaddr, netmask, gw;
    struct netif *netif;

    // 初始化lwIP
    lwip_init();

    // 配置网络接口
    IP4_ADDR(&ipaddr, 192, 168, 1, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);

    netif = netif_add(&netif, &ipaddr, &netmask, &gw,
                      NULL, ethernetif_init, ethernet_input);
    
    netif_set_default(netif);
    netif_set_up(netif);

    // 启动DHCP
    dhcp_start(netif);

    // 创建应用任务
    sys_thread_new("http_server", http_server_thread, NULL,
                   DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
    
    sys_thread_new("mqtt_client", mqtt_client_thread, NULL,
                   DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
} 