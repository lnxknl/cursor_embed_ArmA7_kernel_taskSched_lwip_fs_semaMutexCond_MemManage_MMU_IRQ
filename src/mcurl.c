#include "mcurl.h"
#include <stdlib.h>
#include <string.h>

// CURL句柄结构
struct mcurl_handle {
    mcurl_options_t options;
    mcurl_progress_callback progress_cb;
    void *progress_data;
    mcurl_write_callback write_cb;
    void *write_data;
    mcurl_read_callback read_cb;
    void *read_data;
    char error[256];
};

// 初始化CURL
mcurl_t *mcurl_init(void)
{
    mcurl_t *curl = calloc(1, sizeof(mcurl_t));
    if (!curl) return NULL;
    
    // 设置默认选项
    curl->options.method = MCURL_GET;
    curl->options.ssl_version = MCURL_SSL_ANY;
    curl->options.verify_ssl = true;
    curl->options.connect_timeout = 30;
    curl->options.timeout = 300;
    curl->options.follow_location = true;
    curl->options.max_redirects = 50;
    
    return curl;
}

// 清理CURL
void mcurl_cleanup(mcurl_t *curl)
{
    if (!curl) return;
    
    // 释放选项
    free(curl->options.proxy_host);
    free(curl->options.proxy_user);
    free(curl->options.proxy_pass);
    free(curl->options.ca_path);
    free(curl->options.client_cert);
    free(curl->options.client_key);
    free(curl->options.user_agent);
    free(curl->options.cookie);
    
    // 释放头部
    mcurl_header_t *header = curl->options.headers;
    while (header) {
        mcurl_header_t *next = header->next;
        free(header->name);
        free(header->value);
        free(header);
        header = next;
    }
    
    free(curl);
}

// 设置长整型选项
int mcurl_setopt_long(mcurl_t *curl,
                     const char *option,
                     long value)
{
    if (!curl || !option) return MCURL_ERROR_INIT;
    
    if (strcmp(option, "CONNECT_TIMEOUT") == 0) {
        curl->options.connect_timeout = value;
    } else if (strcmp(option, "TIMEOUT") == 0) {
        curl->options.timeout = value;
    } else if (strcmp(option, "FOLLOW_LOCATION") == 0) {
        curl->options.follow_location = value != 0;
    } else if (strcmp(option, "MAX_REDIRECTS") == 0) {
        curl->options.max_redirects = value;
    } else if (strcmp(option, "VERIFY_SSL") == 0) {
        curl->options.verify_ssl = value != 0;
    } else if (strcmp(option, "VERBOSE") == 0) {
        curl->options.verbose = value != 0;
    } else {
        return MCURL_ERROR_INIT;
    }
    
    return MCURL_OK;
}

// 设置字符串选项
int mcurl_setopt_string(mcurl_t *curl,
                       const char *option,
                       const char *value)
{
    if (!curl || !option) return MCURL_ERROR_INIT;
    
    char **target = NULL;
    
    if (strcmp(option, "PROXY") == 0) {
        target = &curl->options.proxy_host;
    } else if (strcmp(option, "PROXY_USER") == 0) {
        target = &curl->options.proxy_user;
    } else if (strcmp(option, "PROXY_PASS") == 0) {
        target = &curl->options.proxy_pass;
    } else if (strcmp(option, "CA_PATH") == 0) {
        target = &curl->options.ca_path;
    } else if (strcmp(option, "CLIENT_CERT") == 0) {
        target = &curl->options.client_cert;
    } else if (strcmp(option, "CLIENT_KEY") == 0) {
        target = &curl->options.client_key;
    } else if (strcmp(option, "USER_AGENT") == 0) {
        target = &curl->options.user_agent;
    } else if (strcmp(option, "COOKIE") == 0) {
        target = &curl->options.cookie;
    } else {
        return MCURL_ERROR_INIT;
    }
    
    free(*target);
    *target = value ? strdup(value) : NULL;
    
    return MCURL_OK;
}

// 设置指针选项
int mcurl_setopt_pointer(mcurl_t *curl,
                        const char *option,
                        void *value)
{
    if (!curl || !option) return MCURL_ERROR_INIT;
    
    if (strcmp(option, "PROGRESS_CALLBACK") == 0) {
        curl->progress_cb = value;
    } else if (strcmp(option, "PROGRESS_DATA") == 0) {
        curl->progress_data = value;
    } else if (strcmp(option, "WRITE_CALLBACK") == 0) {
        curl->write_cb = value;
    } else if (strcmp(option, "WRITE_DATA") == 0) {
        curl->write_data = value;
    } else if (strcmp(option, "READ_CALLBACK") == 0) {
        curl->read_cb = value;
    } else if (strcmp(option, "READ_DATA") == 0) {
        curl->read_data = value;
    } else {
        return MCURL_ERROR_INIT;
    }
    
    return MCURL_OK;
} 