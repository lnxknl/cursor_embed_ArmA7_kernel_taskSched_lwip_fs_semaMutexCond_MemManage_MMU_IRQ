#include "kvdb.h"
#include <stdlib.h>
#include <string.h>

// 事务操作类型
typedef enum {
    TXN_OP_PUT,
    TXN_OP_DELETE
} txn_op_type_t;

// 事务操作记录
typedef struct txn_op {
    txn_op_type_t type;
    void *key;
    size_t key_len;
    void *value;
    size_t value_len;
    struct txn_op *next;
} txn_op_t;

// 事务结构
struct kvdb_txn {
    kvdb_t *db;
    txn_op_t *ops;
    size_t op_count;
    pthread_mutex_t lock;
};

// 创建事务
kvdb_txn_t *kvdb_txn_begin(kvdb_t *db)
{
    kvdb_txn_t *txn = calloc(1, sizeof(kvdb_txn_t));
    if (!txn) return NULL;
    
    txn->db = db;
    pthread_mutex_init(&txn->lock, NULL);
    
    return txn;
}

// 添加事务操作
static int txn_add_op(kvdb_txn_t *txn, txn_op_type_t type,
                      const void *key, size_t key_len,
                      const void *value, size_t value_len)
{
    txn_op_t *op = calloc(1, sizeof(txn_op_t));
    if (!op) return KVDB_ERR_IO;
    
    op->type = type;
    
    op->key = malloc(key_len);
    if (!op->key) {
        free(op);
        return KVDB_ERR_IO;
    }
    memcpy(op->key, key, key_len);
    op->key_len = key_len;
    
    if (value && value_len > 0) {
        op->value = malloc(value_len);
        if (!op->value) {
            free(op->key);
            free(op);
            return KVDB_ERR_IO;
        }
        memcpy(op->value, value, value_len);
        op->value_len = value_len;
    }
    
    pthread_mutex_lock(&txn->lock);
    
    op->next = txn->ops;
    txn->ops = op;
    txn->op_count++;
    
    pthread_mutex_unlock(&txn->lock);
    return KVDB_OK;
}

// 事务Get操作
int kvdb_txn_get(kvdb_txn_t *txn,
                 const void *key, size_t key_len,
                 void **value, size_t *value_len)
{
    pthread_mutex_lock(&txn->lock);
    
    // 首先在事务操作列表中查找
    txn_op_t *op = txn->ops;
    while (op) {
        if (op->key_len == key_len &&
            memcmp(op->key, key, key_len) == 0) {
            if (op->type == TXN_OP_DELETE) {
                pthread_mutex_unlock(&txn->lock);
                return KVDB_ERR_NOTFOUND;
            }
            *value = malloc(op->value_len);
            if (!*value) {
                pthread_mutex_unlock(&txn->lock);
                return KVDB_ERR_IO;
            }
            memcpy(*value, op->value, op->value_len);
            *value_len = op->value_len;
            pthread_mutex_unlock(&txn->lock);
            return KVDB_OK;
        }
        op = op->next;
    }
    
    pthread_mutex_unlock(&txn->lock);
    
    // 未找到则从数据库查找
    return kvdb_get(txn->db, key, key_len, value, value_len);
}

// 事务Put操作
int kvdb_txn_put(kvdb_txn_t *txn,
                 const void *key, size_t key_len,
                 const void *value, size_t value_len)
{
    return txn_add_op(txn, TXN_OP_PUT, key, key_len, value, value_len);
}

// 事务Delete操作
int kvdb_txn_delete(kvdb_txn_t *txn,
                    const void *key, size_t key_len)
{
    return txn_add_op(txn, TXN_OP_DELETE, key, key_len, NULL, 0);
}

// 提交事务
int kvdb_txn_commit(kvdb_txn_t *txn)
{
    int ret = KVDB_OK;
    pthread_mutex_lock(&txn->lock);
    
    // 按顺序执行所有操作
    txn_op_t *op = txn->ops;
    while (op) {
        if (op->type == TXN_OP_PUT) {
            ret = kvdb_put(txn->db, op->key, op->key_len,
                          op->value, op->value_len);
        } else {
            ret = kvdb_delete(txn->db, op->key, op->key_len);
        }
        
        if (ret != KVDB_OK) break;
        op = op->next;
    }
    
    pthread_mutex_unlock(&txn->lock);
    
    // 清理事务
    kvdb_txn_abort(txn);
    return ret;
}

// 中止事务
void kvdb_txn_abort(kvdb_txn_t *txn)
{
    pthread_mutex_lock(&txn->lock);
    
    // 释放所有操作
    txn_op_t *op = txn->ops;
    while (op) {
        txn_op_t *next = op->next;
        free(op->key);
        free(op->value);
        free(op);
        op = next;
    }
    
    txn->ops = NULL;
    txn->op_count = 0;
    
    pthread_mutex_unlock(&txn->lock);
    pthread_mutex_destroy(&txn->lock);
    free(txn);
} 