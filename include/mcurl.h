#ifndef MCURL_H
#define MCURL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// 错误码定义
#define MCURL_OK               0
#define MCURL_ERROR_INIT      -1
#define MCURL_ERROR_PROTOCOL  -2
#define MCURL_ERROR_CONNECT   -3
#define MCURL_ERROR_TIMEOUT   -4
#define MCURL_ERROR_AUTH      -5
#define MCURL_ERROR_SSL       -6
#define MCURL_ERROR_MEMORY    -7

// HTTP方法
typedef enum {
    MCURL_GET,
    MCURL_POST,
    MCURL_PUT,
    MCURL_DELETE,
    MCURL_HEAD,
    MCURL_OPTIONS,
    MCURL_PATCH
} mcurl_method_t;

// SSL版本
typedef enum {
    MCURL_SSL_ANY,
    MCURL_SSL_TLSv1,
    MCURL_SSL_TLSv1_1,
    MCURL_SSL_TLSv1_2,
    MCURL_SSL_TLSv1_3
} mcurl_ssl_version_t;

// 代理类型
typedef enum {
    MCURL_PROXY_NONE,
    MCURL_PROXY_HTTP,
    MCURL_PROXY_SOCKS4,
    MCURL_PROXY_SOCKS5
} mcurl_proxy_type_t;

// HTTP头部
typedef struct mcurl_header {
    char *name;
    char *value;
    struct mcurl_header *next;
} mcurl_header_t;

// 请求选项
typedef struct {
    mcurl_method_t method;
    mcurl_ssl_version_t ssl_version;
    mcurl_proxy_type_t proxy_type;
    char *proxy_host;
    uint16_t proxy_port;
    char *proxy_user;
    char *proxy_pass;
    bool verify_ssl;
    char *ca_path;
    char *client_cert;
    char *client_key;
    mcurl_header_t *headers;
    char *user_agent;
    char *cookie;
    long connect_timeout;
    long timeout;
    bool follow_location;
    long max_redirects;
    bool verbose;
} mcurl_options_t;

// 响应结构
typedef struct {
    int status_code;
    mcurl_header_t *headers;
    char *body;
    size_t body_length;
    char *error;
} mcurl_response_t;

// 进度回调
typedef int (*mcurl_progress_callback)(void *userdata,
                                     double dltotal,
                                     double dlnow,
                                     double ultotal,
                                     double ulnow);

// 写入回调
typedef size_t (*mcurl_write_callback)(char *ptr,
                                     size_t size,
                                     size_t nmemb,
                                     void *userdata);

// 读取回调
typedef size_t (*mcurl_read_callback)(char *ptr,
                                    size_t size,
                                    size_t nmemb,
                                    void *userdata);

// CURL句柄
typedef struct mcurl_handle mcurl_t;

// API函数声明
mcurl_t *mcurl_init(void);
void mcurl_cleanup(mcurl_t *curl);

int mcurl_setopt_long(mcurl_t *curl, const char *option, long value);
int mcurl_setopt_string(mcurl_t *curl, const char *option, const char *value);
int mcurl_setopt_pointer(mcurl_t *curl, const char *option, void *value);

int mcurl_perform(mcurl_t *curl, const char *url, mcurl_response_t *response);
void mcurl_free_response(mcurl_response_t *response);

// 高级功能
int mcurl_multi_init(void);
int mcurl_multi_add_handle(mcurl_t *curl);
int mcurl_multi_remove_handle(mcurl_t *curl);
int mcurl_multi_perform(void);
void mcurl_multi_cleanup(void);

#endif 