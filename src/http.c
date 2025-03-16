#include "mcurl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_HEADER_SIZE 8192
#define MAX_LINE_LENGTH 1024

// HTTP请求结构
typedef struct {
    mcurl_method_t method;
    char *url;
    char *host;
    uint16_t port;
    char *path;
    mcurl_header_t *headers;
    char *body;
    size_t body_length;
} http_request_t;

// 解析URL
static int parse_url(const char *url,
                    char **host,
                    uint16_t *port,
                    char **path,
                    bool *use_ssl)
{
    // 检查协议
    if (strncmp(url, "http://", 7) == 0) {
        *use_ssl = false;
        url += 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        *use_ssl = true;
        url += 8;
    } else {
        return MCURL_ERROR_PROTOCOL;
    }
    
    // 查找主机结束位置
    const char *host_end = strchr(url, '/');
    if (!host_end) {
        host_end = url + strlen(url);
        *path = strdup("/");
    } else {
        *path = strdup(host_end);
    }
    
    // 解析主机和端口
    char host_str[256];
    size_t host_len = host_end - url;
    if (host_len >= sizeof(host_str)) {
        return MCURL_ERROR_PROTOCOL;
    }
    
    strncpy(host_str, url, host_len);
    host_str[host_len] = '\0';
    
    // 查找端口号
    char *port_str = strchr(host_str, ':');
    if (port_str) {
        *port_str = '\0';
        port_str++;
        *port = atoi(port_str);
    } else {
        *port = *use_ssl ? 443 : 80;
    }
    
    *host = strdup(host_str);
    return MCURL_OK;
}

// 构建HTTP请求
static char *build_request(const http_request_t *request,
                         const mcurl_options_t *options,
                         size_t *length)
{
    // 计算请求大小
    size_t size = MAX_HEADER_SIZE;
    if (request->body) {
        size += request->body_length;
    }
    
    char *buffer = malloc(size);
    if (!buffer) return NULL;
    
    // 添加请求行
    const char *method_str;
    switch (request->method) {
        case MCURL_GET:     method_str = "GET"; break;
        case MCURL_POST:    method_str = "POST"; break;
        case MCURL_PUT:     method_str = "PUT"; break;
        case MCURL_DELETE:  method_str = "DELETE"; break;
        case MCURL_HEAD:    method_str = "HEAD"; break;
        case MCURL_OPTIONS: method_str = "OPTIONS"; break;
        case MCURL_PATCH:   method_str = "PATCH"; break;
        default:            method_str = "GET"; break;
    }
    
    int pos = snprintf(buffer, size,
                      "%s %s HTTP/1.1\r\n"
                      "Host: %s\r\n",
                      method_str,
                      request->path,
                      request->host);
    
    // 添加自定义头部
    mcurl_header_t *header = options->headers;
    while (header) {
        pos += snprintf(buffer + pos, size - pos,
                       "%s: %s\r\n",
                       header->name,
                       header->value);
        header = header->next;
    }
    
    // 添加标准头部
    if (options->user_agent) {
        pos += snprintf(buffer + pos, size - pos,
                       "User-Agent: %s\r\n",
                       options->user_agent);
    }
    
    if (options->cookie) {
        pos += snprintf(buffer + pos, size - pos,
                       "Cookie: %s\r\n",
                       options->cookie);
    }
    
    if (request->body) {
        pos += snprintf(buffer + pos, size - pos,
                       "Content-Length: %zu\r\n",
                       request->body_length);
    }
    
    // 结束头部
    pos += snprintf(buffer + pos, size - pos, "\r\n");
    
    // 添加请求体
    if (request->body) {
        memcpy(buffer + pos, request->body, request->body_length);
        pos += request->body_length;
    }
    
    *length = pos;
    return buffer;
}

// 解析HTTP响应
static int parse_response(const char *buffer,
                        size_t length,
                        mcurl_response_t *response)
{
    // 解析状态行
    const char *pos = buffer;
    const char *end = buffer + length;
    
    // 跳过HTTP版本
    pos = strchr(pos, ' ');
    if (!pos || pos >= end) return MCURL_ERROR_PROTOCOL;
    pos++;
    
    // 解析状态码
    response->status_code = atoi(pos);
    
    // 查找头部开始
    pos = strstr(pos, "\r\n");
    if (!pos || pos >= end) return MCURL_ERROR_PROTOCOL;
    pos += 2;
    
    // 解析头部
    char line[MAX_LINE_LENGTH];
    while (pos < end) {
        // 读取一行
        const char *line_end = strstr(pos, "\r\n");
        if (!line_end || line_end >= end) break;
        
        size_t line_length = line_end - pos;
        if (line_length == 0) {
            // 头部结束
            pos = line_end + 2;
            break;
        }
        
        if (line_length >= sizeof(line)) {
            return MCURL_ERROR_PROTOCOL;
        }
        
        memcpy(line, pos, line_length);
        line[line_length] = '\0';
        
        // 解析头部字段
        char *sep = strchr(line, ':');
        if (sep) {
            *sep = '\0';
            sep++;
            while (*sep == ' ') sep++;
            
            mcurl_header_t *header = malloc(sizeof(mcurl_header_t));
            header->name = strdup(line);
            header->value = strdup(sep);
            header->next = response->headers;
            response->headers = header;
        }
        
        pos = line_end + 2;
    }
    
    // 复制响应体
    size_t body_length = end - pos;
    if (body_length > 0) {
        response->body = malloc(body_length);
        memcpy(response->body, pos, body_length);
        response->body_length = body_length;
    }
    
    return MCURL_OK;
} 