#include "mcurl.h"
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// 网络连接结构
typedef struct {
    int socket;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    bool is_ssl;
    struct sockaddr_in addr;
    char *host;
    uint16_t port;
} network_conn_t;

// 创建网络连接
static network_conn_t *create_connection(const char *host,
                                      uint16_t port,
                                      bool use_ssl,
                                      const mcurl_options_t *options)
{
    network_conn_t *conn = calloc(1, sizeof(network_conn_t));
    if (!conn) return NULL;
    
    conn->host = strdup(host);
    conn->port = port;
    conn->is_ssl = use_ssl;
    
    // 解析主机名
    struct hostent *he = gethostbyname(host);
    if (!he) {
        free(conn->host);
        free(conn);
        return NULL;
    }
    
    // 设置地址
    memset(&conn->addr, 0, sizeof(conn->addr));
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_port = htons(port);
    memcpy(&conn->addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    // 创建socket
    conn->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->socket < 0) {
        free(conn->host);
        free(conn);
        return NULL;
    }
    
    // 设置非阻塞
    int flags = fcntl(conn->socket, F_GETFL, 0);
    fcntl(conn->socket, F_SETFL, flags | O_NONBLOCK);
    
    // 初始化SSL
    if (use_ssl) {
        SSL_library_init();
        SSL_load_error_strings();
        
        const SSL_METHOD *method = TLS_client_method();
        conn->ssl_ctx = SSL_CTX_new(method);
        if (!conn->ssl_ctx) {
            close(conn->socket);
            free(conn->host);
            free(conn);
            return NULL;
        }
        
        // 设置SSL选项
        if (options->verify_ssl) {
            SSL_CTX_set_verify(conn->ssl_ctx, SSL_VERIFY_PEER, NULL);
            if (options->ca_path) {
                SSL_CTX_load_verify_locations(conn->ssl_ctx,
                                           options->ca_path,
                                           NULL);
            }
        }
        
        if (options->client_cert && options->client_key) {
            SSL_CTX_use_certificate_file(conn->ssl_ctx,
                                      options->client_cert,
                                      SSL_FILETYPE_PEM);
            SSL_CTX_use_PrivateKey_file(conn->ssl_ctx,
                                     options->client_key,
                                     SSL_FILETYPE_PEM);
        }
        
        conn->ssl = SSL_new(conn->ssl_ctx);
        SSL_set_fd(conn->ssl, conn->socket);
    }
    
    return conn;
}

// 连接到服务器
static int connect_to_server(network_conn_t *conn,
                           long connect_timeout)
{
    // 开始连接
    int ret = connect(conn->socket,
                     (struct sockaddr *)&conn->addr,
                     sizeof(conn->addr));
    
    if (ret < 0 && errno != EINPROGRESS) {
        return MCURL_ERROR_CONNECT;
    }
    
    // 等待连接完成
    fd_set write_fds;
    struct timeval tv;
    
    FD_ZERO(&write_fds);
    FD_SET(conn->socket, &write_fds);
    
    tv.tv_sec = connect_timeout;
    tv.tv_usec = 0;
    
    ret = select(conn->socket + 1, NULL, &write_fds, NULL, &tv);
    if (ret <= 0) {
        return MCURL_ERROR_TIMEOUT;
    }
    
    // 检查连接状态
    int error;
    socklen_t len = sizeof(error);
    getsockopt(conn->socket, SOL_SOCKET, SO_ERROR, &error, &len);
    if (error != 0) {
        return MCURL_ERROR_CONNECT;
    }
    
    // SSL握手
    if (conn->is_ssl) {
        ret = SSL_connect(conn->ssl);
        if (ret != 1) {
            return MCURL_ERROR_SSL;
        }
    }
    
    return MCURL_OK;
}

// 发送数据
static int send_data(network_conn_t *conn,
                    const char *data,
                    size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t ret;
        if (conn->is_ssl) {
            ret = SSL_write(conn->ssl, data + sent, length - sent);
        } else {
            ret = send(conn->socket, data + sent, length - sent, 0);
        }
        
        if (ret <= 0) {
            return MCURL_ERROR_CONNECT;
        }
        
        sent += ret;
    }
    
    return MCURL_OK;
}

// 接收数据
static int recv_data(network_conn_t *conn,
                    char *buffer,
                    size_t size,
                    size_t *received)
{
    ssize_t ret;
    if (conn->is_ssl) {
        ret = SSL_read(conn->ssl, buffer, size);
    } else {
        ret = recv(conn->socket, buffer, size, 0);
    }
    
    if (ret < 0) {
        return MCURL_ERROR_CONNECT;
    }
    
    *received = ret;
    return MCURL_OK;
} 