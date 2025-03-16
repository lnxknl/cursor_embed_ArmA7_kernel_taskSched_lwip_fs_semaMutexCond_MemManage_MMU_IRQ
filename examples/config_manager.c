#include "cjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 配置结构体 */
typedef struct {
    /* 系统配置 */
    struct {
        char hostname[64];
        int port;
        char log_level[16];
        char log_file[256];
    } system;

    /* 数据库配置 */
    struct {
        char host[64];
        int port;
        char username[32];
        char password[32];
        char database[32];
        int max_connections;
    } database;

    /* 缓存配置 */
    struct {
        char host[64];
        int port;
        int timeout;
        size_t max_memory;
    } cache;

    /* 安全配置 */
    struct {
        bool ssl_enabled;
        char cert_file[256];
        char key_file[256];
        char ca_file[256];
    } security;
} config_t;

/* 加载配置文件 */
static config_t *load_config(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    /* 读取文件内容 */
    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = (char*)malloc(length + 1);
    if (!data)
    {
        fclose(fp);
        return NULL;
    }

    fread(data, 1, length, fp);
    data[length] = '\0';
    fclose(fp);

    /* 解析JSON */
    cJSON *json = cJSON_Parse(data);
    free(data);

    if (!json) return NULL;

    /* 创建配置对象 */
    config_t *config = (config_t*)calloc(1, sizeof(config_t));
    if (!config)
    {
        cJSON_Delete(json);
        return NULL;
    }

    /* 解析系统配置 */
    cJSON *system = cJSON_GetObjectItem(json, "system");
    if (system)
    {
        cJSON *hostname = cJSON_GetObjectItem(system, "hostname");
        cJSON *port = cJSON_GetObjectItem(system, "port");
        cJSON *log_level = cJSON_GetObjectItem(system, "log_level");
        cJSON *log_file = cJSON_GetObjectItem(system, "log_file");

        if (hostname && hostname->valuestring)
            strncpy(config->system.hostname, hostname->valuestring, sizeof(config->system.hostname) - 1);
        if (port) config->system.port = port->valueint;
        if (log_level && log_level->valuestring)
            strncpy(config->system.log_level, log_level->valuestring, sizeof(config->system.log_level) - 1);
        if (log_file && log_file->valuestring)
            strncpy(config->system.log_file, log_file->valuestring, sizeof(config->system.log_file) - 1);
    }

    /* 解析数据库配置 */
    cJSON *database = cJSON_GetObjectItem(json, "database");
    if (database)
    {
        cJSON *host = cJSON_GetObjectItem(database, "host");
        cJSON *port = cJSON_GetObjectItem(database, "port");
        cJSON *username = cJSON_GetObjectItem(database, "username");
        cJSON *password = cJSON_GetObjectItem(database, "password");
        cJSON *db = cJSON_GetObjectItem(database, "database");
        cJSON *max_conn = cJSON_GetObjectItem(database, "max_connections");

        if (host && host->valuestring)
            strncpy(config->database.host, host->valuestring, sizeof(config->database.host) - 1);
        if (port) config->database.port = port->valueint;
        if (username && username->valuestring)
            strncpy(config->database.username, username->valuestring, sizeof(config->database.username) - 1);
        if (password && password->valuestring)
            strncpy(config->database.password, password->valuestring, sizeof(config->database.password) - 1);
        if (db && db->valuestring)
            strncpy(config->database.database, db->valuestring, sizeof(config->database.database) - 1);
        if (max_conn) config->database.max_connections = max_conn->valueint;
    }

    /* 解析缓存配置 */
    cJSON *cache = cJSON_GetObjectItem(json, "cache");
    if (cache)
    {
        cJSON *host = cJSON_GetObjectItem(cache, "host");
        cJSON *port = cJSON_GetObjectItem(cache, "port");
        cJSON *timeout = cJSON_GetObjectItem(cache, "timeout");
        cJSON *max_memory = cJSON_GetObjectItem(cache, "max_memory");

        if (host && host->valuestring)
            strncpy(config->cache.host, host->valuestring, sizeof(config->cache.host) - 1);
        if (port) config->cache.port = port->valueint;
        if (timeout) config->cache.timeout = timeout->valueint;
        if (max_memory) config->cache.max_memory = max_memory->valueint;
    }

    /* 解析安全配置 */
    cJSON *security = cJSON_GetObjectItem(json, "security");
    if (security)
    {
        cJSON *ssl_enabled = cJSON_GetObjectItem(security, "ssl_enabled");
        cJSON *cert_file = cJSON_GetObjectItem(security, "cert_file");
        cJSON *key_file = cJSON_GetObjectItem(security, "key_file");
        cJSON *ca_file = cJSON_GetObjectItem(security, "ca_file");

        if (ssl_enabled) config->security.ssl_enabled = ssl_enabled->valueint != 0;
        if (cert_file && cert_file->valuestring)
            strncpy(config->security.cert_file, cert_file->valuestring, sizeof(config->security.cert_file) - 1);
        if (key_file && key_file->valuestring)
            strncpy(config->security.key_file, key_file->valuestring, sizeof(config->security.key_file) - 1);
        if (ca_file && ca_file->valuestring)
            strncpy(config->security.ca_file, ca_file->valuestring, sizeof(config->security.ca_file) - 1);
    }

    cJSON_Delete(json);
    return config;
}

/* 保存配置到文件 */
static int save_config(const config_t *config, const char *filename)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;

    /* 添加系统配置 */
    cJSON *system = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "system", system);
    cJSON_AddStringToObject(system, "hostname", config->system.hostname);
    cJSON_AddNumberToObject(system, "port", config->system.port);
    cJSON_AddStringToObject(system, "log_level", config->system.log_level);
    cJSON_AddStringToObject(system, "log_file", config->system.log_file);

    /* 添加数据库配置 */
    cJSON *database = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "database", database);
    cJSON_AddStringToObject(database, "host", config->database.host);
    cJSON_AddNumberToObject(database, "port", config->database.port);
    cJSON_AddStringToObject(database, "username", config->database.username);
    cJSON_AddStringToObject(database, "password", config->database.password);
    cJSON_AddStringToObject(database, "database", config->database.database);
    cJSON_AddNumberToObject(database, "max_connections", config->database.max_connections);

    /* 添加缓存配置 */
    cJSON *cache = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "cache", cache);
    cJSON_AddStringToObject(cache, "host", config->cache.host);
    cJSON_AddNumberToObject(cache, "port", config->cache.port);
    cJSON_AddNumberToObject(cache, "timeout", config->cache.timeout);
    cJSON_AddNumberToObject(cache, "max_memory", config->cache.max_memory);

    /* 添加安全配置 */
    cJSON *security = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "security", security);
    cJSON_AddBoolToObject(security, "ssl_enabled", config->security.ssl_enabled);
    cJSON_AddStringToObject(security, "cert_file", config->security.cert_file);
    cJSON_AddStringToObject(security, "key_file", config->security.key_file);
    cJSON_AddStringToObject(security, "ca_file", config->security.ca_file);

    /* 生成JSON字符串 */
    char *str = cJSON_Print(json);
    cJSON_Delete(json);

    if (!str) return -1;

    /* 写入文件 */
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        free(str);
        return -1;
    }

    fprintf(fp, "%s\n", str);
    free(str);
    fclose(fp);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <config_file>\n", argv[0]);
        return 1;
    }

    /* 加载配置 */
    config_t *config = load_config(argv[1]);
    if (!config)
    {
        printf("Failed to load configuration from %s\n", argv[1]);
        return 1;
    }

    /* 显示当前配置 */
    printf("Current configuration:\n\n");
    printf("System:\n");
    printf("  Hostname: %s\n", config->system.hostname);
    printf("  Port: %d\n", config->system.port);
    printf("  Log Level: %s\n", config->system.log_level);
    printf("  Log File: %s\n\n", config->system.log_file);

    printf("Database:\n");
    printf("  Host: %s\n", config->database.host);
    printf("  Port: %d\n", config->database.port);
    printf("  Username: %s\n", config->database.username);
    printf("  Database: %s\n", config->database.database);
    printf("  Max Connections: %d\n\n", config->database.max_connections);

    printf("Cache:\n");
    printf("  Host: %s\n", config->cache.host);
    printf("  Port: %d\n", config->cache.port);
    printf("  Timeout: %d\n", config->cache.timeout);
    printf("  Max Memory: %zu\n\n", config->cache.max_memory);

    printf("Security:\n");
    printf("  SSL Enabled: %s\n", config->security.ssl_enabled ? "yes" : "no");
    printf("  Cert File: %s\n", config->security.cert_file);
    printf("  Key File: %s\n", config->security.key_file);
    printf("  CA File: %s\n", config->security.ca_file);

    /* 修改配置示例 */
    strncpy(config->system.hostname, "new-host", sizeof(config->system.hostname) - 1);
    config->database.max_connections = 100;
    config->cache.timeout = 5000;

    /* 保存修改后的配置 */
    if (save_config(config, argv[1]) != 0)
    {
        printf("\nFailed to save configuration to %s\n", argv[1]);
        free(config);
        return 1;
    }

    printf("\nConfiguration has been updated and saved.\n");

    free(config);
    return 0;
} 