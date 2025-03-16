#include "kvdb.h"
#include <stdlib.h>
#include <string.h>

// 跳表最大层数
#define MAX_LEVEL 32

// 跳表节点结构
typedef struct skiplist_node {
    void *key;              // 键
    size_t key_len;         // 键长度
    off_t value_offset;     // 值偏移
    size_t value_len;       // 值长度
    int level;              // 节点层数
    struct skiplist_node *forward[MAX_LEVEL];  // 前向指针
} skiplist_node_t;

// 跳表结构
typedef struct {
    skiplist_node_t *header;    // 头节点
    int level;                  // 当前最大层数
    size_t size;               // 节点数量
    kvdb_compare_func compare;  // 比较函数
    pthread_mutex_t lock;       // 跳表锁
} skiplist_t;

// 创建跳表节点
static skiplist_node_t *create_node(int level,
                                  const void *key, size_t key_len,
                                  off_t value_offset, size_t value_len)
{
    skiplist_node_t *node = calloc(1, sizeof(skiplist_node_t) +
                                  level * sizeof(skiplist_node_t*));
    if (!node) return NULL;
    
    node->key = malloc(key_len);
    if (!node->key) {
        free(node);
        return NULL;
    }
    
    memcpy(node->key, key, key_len);
    node->key_len = key_len;
    node->value_offset = value_offset;
    node->value_len = value_len;
    node->level = level;
    
    return node;
}

// 生成随机层数
static int random_level(void)
{
    int level = 1;
    while (rand() < RAND_MAX/2 && level < MAX_LEVEL)
        level++;
    return level;
}

// 创建跳表
skiplist_t *skiplist_create(kvdb_compare_func compare)
{
    skiplist_t *list = calloc(1, sizeof(skiplist_t));
    if (!list) return NULL;
    
    list->header = create_node(MAX_LEVEL, NULL, 0, 0, 0);
    if (!list->header) {
        free(list);
        return NULL;
    }
    
    list->level = 1;
    list->compare = compare;
    pthread_mutex_init(&list->lock, NULL);
    
    return list;
}

// 查找键
static skiplist_node_t *skiplist_find(skiplist_t *list,
                                    const void *key, size_t key_len)
{
    skiplist_node_t *node = list->header;
    
    for (int i = list->level - 1; i >= 0; i--) {
        while (node->forward[i] &&
               list->compare(node->forward[i]->key,
                           node->forward[i]->key_len,
                           key, key_len) < 0) {
            node = node->forward[i];
        }
    }
    
    node = node->forward[0];
    
    if (node && list->compare(node->key, node->key_len,
                             key, key_len) == 0) {
        return node;
    }
    
    return NULL;
}

// 插入键值对
int skiplist_insert(skiplist_t *list,
                   const void *key, size_t key_len,
                   off_t value_offset, size_t value_len)
{
    skiplist_node_t *update[MAX_LEVEL];
    skiplist_node_t *node = list->header;
    
    pthread_mutex_lock(&list->lock);
    
    // 查找插入位置
    for (int i = list->level - 1; i >= 0; i--) {
        while (node->forward[i] &&
               list->compare(node->forward[i]->key,
                           node->forward[i]->key_len,
                           key, key_len) < 0) {
            node = node->forward[i];
        }
        update[i] = node;
    }
    
    node = node->forward[0];
    
    // 键已存在，更新值
    if (node && list->compare(node->key, node->key_len,
                             key, key_len) == 0) {
        node->value_offset = value_offset;
        node->value_len = value_len;
        pthread_mutex_unlock(&list->lock);
        return KVDB_OK;
    }
    
    // 生成新节点
    int level = random_level();
    if (level > list->level) {
        for (int i = list->level; i < level; i++) {
            update[i] = list->header;
        }
        list->level = level;
    }
    
    node = create_node(level, key, key_len, value_offset, value_len);
    if (!node) {
        pthread_mutex_unlock(&list->lock);
        return KVDB_ERR_IO;
    }
    
    // 更新指针
    for (int i = 0; i < level; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }
    
    list->size++;
    pthread_mutex_unlock(&list->lock);
    return KVDB_OK;
} 