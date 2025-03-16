#ifndef KVDB_H
#define KVDB_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// 错误码定义
#define KVDB_OK             0
#define KVDB_ERR_IO        -1
#define KVDB_ERR_CORRUPT   -2
#define KVDB_ERR_NOTFOUND  -3
#define KVDB_ERR_FULL      -4
#define KVDB_ERR_EXIST     -5
#define KVDB_ERR_BUSY      -6

// 数据库选项
typedef struct {
    size_t block_size;      // 数据块大小
    size_t cache_size;      // 缓存大小
    bool sync_write;        // 是否同步写入
    const char *data_dir;   // 数据目录
    uint32_t max_file_size; // 最大文件大小
} kvdb_options_t;

// 键值对结构
typedef struct {
    void *key;
    size_t key_len;
    void *value;
    size_t value_len;
} kvdb_entry_t;

// 迭代器结构
typedef struct kvdb_iterator kvdb_iterator_t;

// 事务结构
typedef struct kvdb_txn kvdb_txn_t;

// 数据库句柄
typedef struct kvdb_handle kvdb_t;

// 比较函数类型
typedef int (*kvdb_compare_func)(const void *a, size_t alen,
                                const void *b, size_t blen);

// API函数声明
kvdb_t *kvdb_open(const kvdb_options_t *options);
void kvdb_close(kvdb_t *db);

int kvdb_get(kvdb_t *db, const void *key, size_t key_len,
             void **value, size_t *value_len);
int kvdb_put(kvdb_t *db, const void *key, size_t key_len,
             const void *value, size_t value_len);
int kvdb_delete(kvdb_t *db, const void *key, size_t key_len);

// 事务操作
kvdb_txn_t *kvdb_txn_begin(kvdb_t *db);
int kvdb_txn_commit(kvdb_txn_t *txn);
void kvdb_txn_abort(kvdb_txn_t *txn);

int kvdb_txn_get(kvdb_txn_t *txn, const void *key, size_t key_len,
                 void **value, size_t *value_len);
int kvdb_txn_put(kvdb_txn_t *txn, const void *key, size_t key_len,
                 const void *value, size_t value_len);
int kvdb_txn_delete(kvdb_txn_t *txn, const void *key, size_t key_len);

// 迭代器操作
kvdb_iterator_t *kvdb_iterator_new(kvdb_t *db);
void kvdb_iterator_free(kvdb_iterator_t *iter);
void kvdb_iterator_seek(kvdb_iterator_t *iter,
                       const void *key, size_t key_len);
bool kvdb_iterator_valid(kvdb_iterator_t *iter);
void kvdb_iterator_next(kvdb_iterator_t *iter);
void kvdb_iterator_prev(kvdb_iterator_t *iter);
const kvdb_entry_t *kvdb_iterator_entry(kvdb_iterator_t *iter);

#endif 