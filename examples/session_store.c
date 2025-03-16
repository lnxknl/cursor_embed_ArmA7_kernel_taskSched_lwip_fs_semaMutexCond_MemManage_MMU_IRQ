#include "kvdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

// 会话结构
typedef struct {
    char user_id[64];
    char username[128];
    time_t login_time;
    time_t last_access;
    char ip_address[64];
    json_object *data;  // 额外数据
} session_t;

// 会话存储
typedef struct {
    kvdb_t *db;
    pthread_mutex_t lock;
} session_store_t;

// 创建会话存储
session_store_t *session_store_create(const char *data_dir)
{
    session_store_t *store = calloc(1, sizeof(session_store_t));
    if (!store) return NULL;
    
    // 配置数据库选项
    kvdb_options_t options = {
        .block_size = 4096,
        .cache_size = 16 * 1024 * 1024,  // 16MB缓存
        .sync_write = true,
        .data_dir = data_dir,
        .max_file_size = 1024 * 1024 * 1024  // 1GB
    };
    
    store->db = kvdb_open(&options);
    if (!store->db) {
        free(store);
        return NULL;
    }
    
    pthread_mutex_init(&store->lock, NULL);
    return store;
}

// 序列化会话
static char *serialize_session(const session_t *session)
{
    json_object *obj = json_object_new_object();
    
    json_object_object_add(obj, "user_id",
        json_object_new_string(session->user_id));
    json_object_object_add(obj, "username",
        json_object_new_string(session->username));
    json_object_object_add(obj, "login_time",
        json_object_new_int64(session->login_time));
    json_object_object_add(obj, "last_access",
        json_object_new_int64(session->last_access));
    json_object_object_add(obj, "ip_address",
        json_object_new_string(session->ip_address));
    
    if (session->data) {
        json_object_object_add(obj, "data", session->data);
    }
    
    const char *json_str = json_object_to_json_string(obj);
    char *result = strdup(json_str);
    
    json_object_put(obj);
    return result;
}

// 反序列化会话
static session_t *deserialize_session(const char *json_str)
{
    session_t *session = calloc(1, sizeof(session_t));
    if (!session) return NULL;
    
    json_object *obj = json_tokener_parse(json_str);
    if (!obj) {
        free(session);
        return NULL;
    }
    
    json_object *value;
    
    if (json_object_object_get_ex(obj, "user_id", &value)) {
        strncpy(session->user_id, json_object_get_string(value),
                sizeof(session->user_id) - 1);
    }
    
    if (json_object_object_get_ex(obj, "username", &value)) {
        strncpy(session->username, json_object_get_string(value),
                sizeof(session->username) - 1);
    }
    
    if (json_object_object_get_ex(obj, "login_time", &value)) {
        session->login_time = json_object_get_int64(value);
    }
    
    if (json_object_object_get_ex(obj, "last_access", &value)) {
        session->last_access = json_object_get_int64(value);
    }
    
    if (json_object_object_get_ex(obj, "ip_address", &value)) {
        strncpy(session->ip_address, json_object_get_string(value),
                sizeof(session->ip_address) - 1);
    }
    
    if (json_object_object_get_ex(obj, "data", &value)) {
        session->data = json_object_get(value);
    }
    
    json_object_put(obj);
    return session;
}

// 创建新会话
char *session_create(session_store_t *store,
                    const char *user_id,
                    const char *username,
                    const char *ip_address)
{
    // 生成会话ID
    char session_id[33];
    unsigned char random[16];
    FILE *f = fopen("/dev/urandom", "rb");
    fread(random, 1, 16, f);
    fclose(f);
    
    for (int i = 0; i < 16; i++) {
        sprintf(session_id + i * 2, "%02x", random[i]);
    }
    session_id[32] = '\0';
    
    // 创建会话对象
    session_t session = {
        .login_time = time(NULL),
        .last_access = time(NULL),
        .data = json_object_new_object()
    };
    
    strncpy(session.user_id, user_id, sizeof(session.user_id) - 1);
    strncpy(session.username, username, sizeof(session.username) - 1);
    strncpy(session.ip_address, ip_address, sizeof(session.ip_address) - 1);
    
    // 序列化并存储会话
    char *data = serialize_session(&session);
    if (!data) return NULL;
    
    pthread_mutex_lock(&store->lock);
    
    int ret = kvdb_put(store->db, session_id, strlen(session_id),
                       data, strlen(data));
    
    pthread_mutex_unlock(&store->lock);
    
    free(data);
    json_object_put(session.data);
    
    if (ret != KVDB_OK) return NULL;
    
    return strdup(session_id);
}

// 获取会话
session_t *session_get(session_store_t *store, const char *session_id)
{
    void *data;
    size_t data_len;
    
    pthread_mutex_lock(&store->lock);
    
    int ret = kvdb_get(store->db, session_id, strlen(session_id),
                       &data, &data_len);
    
    pthread_mutex_unlock(&store->lock);
    
    if (ret != KVDB_OK) return NULL;
    
    session_t *session = deserialize_session(data);
    free(data);
    
    if (session) {
        session->last_access = time(NULL);
        
        // 更新最后访问时间
        char *updated_data = serialize_session(session);
        if (updated_data) {
            pthread_mutex_lock(&store->lock);
            kvdb_put(store->db, session_id, strlen(session_id),
                     updated_data, strlen(updated_data));
            pthread_mutex_unlock(&store->lock);
            free(updated_data);
        }
    }
    
    return session;
} 