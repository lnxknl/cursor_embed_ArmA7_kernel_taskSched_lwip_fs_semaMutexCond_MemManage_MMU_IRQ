#include "kvdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define BLOCK_HEADER_SIZE 16
#define MAX_KEY_SIZE 1024
#define MAX_VALUE_SIZE (1024 * 1024)

// 数据块头部结构
typedef struct {
    uint32_t magic;     // 魔数
    uint32_t checksum;  // 校验和
    uint32_t key_size;  // 键长度
    uint32_t value_size;// 值长度
} block_header_t;

// 数据文件结构
typedef struct {
    int fd;             // 文件描述符
    char *path;         // 文件路径
    size_t size;        // 文件大小
    pthread_mutex_t lock;// 文件锁
} data_file_t;

// 存储引擎结构
typedef struct {
    data_file_t *active_file;  // 当前活动文件
    data_file_t **old_files;   // 旧文件列表
    size_t old_files_count;    // 旧文件数量
    pthread_mutex_t files_lock; // 文件列表锁
} storage_engine_t;

// 计算校验和
static uint32_t calculate_checksum(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint32_t checksum = 0;
    
    while (len--) {
        checksum = (checksum << 8) | (checksum >> 24);
        checksum += *p++;
    }
    
    return checksum;
}

// 创建新数据文件
static data_file_t *create_data_file(const char *dir, uint32_t id)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%08u.dat", dir, id);
    
    data_file_t *file = calloc(1, sizeof(data_file_t));
    if (!file) return NULL;
    
    file->path = strdup(path);
    file->fd = open(path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (file->fd < 0) {
        free(file->path);
        free(file);
        return NULL;
    }
    
    pthread_mutex_init(&file->lock, NULL);
    
    struct stat st;
    if (fstat(file->fd, &st) == 0) {
        file->size = st.st_size;
    }
    
    return file;
}

// 写入数据块
static int write_block(data_file_t *file,
                      const void *key, size_t key_len,
                      const void *value, size_t value_len)
{
    block_header_t header = {
        .magic = 0x19283746,
        .key_size = key_len,
        .value_size = value_len
    };
    
    // 计算数据校验和
    uint32_t checksum = calculate_checksum(key, key_len);
    checksum = calculate_checksum(value, value_len);
    header.checksum = checksum;
    
    pthread_mutex_lock(&file->lock);
    
    // 写入头部
    ssize_t written = write(file->fd, &header, sizeof(header));
    if (written != sizeof(header)) {
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_IO;
    }
    
    // 写入键
    written = write(file->fd, key, key_len);
    if (written != key_len) {
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_IO;
    }
    
    // 写入值
    written = write(file->fd, value, value_len);
    if (written != value_len) {
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_IO;
    }
    
    file->size += sizeof(header) + key_len + value_len;
    
    if (fsync(file->fd) < 0) {
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_IO;
    }
    
    pthread_mutex_unlock(&file->lock);
    return KVDB_OK;
}

// 读取数据块
static int read_block(data_file_t *file, off_t offset,
                     void **key, size_t *key_len,
                     void **value, size_t *value_len)
{
    block_header_t header;
    
    pthread_mutex_lock(&file->lock);
    
    // 读取头部
    ssize_t bytes = pread(file->fd, &header, sizeof(header), offset);
    if (bytes != sizeof(header)) {
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_IO;
    }
    
    // 验证魔数
    if (header.magic != 0x19283746) {
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_CORRUPT;
    }
    
    // 分配内存
    void *key_data = malloc(header.key_size);
    void *value_data = malloc(header.value_size);
    if (!key_data || !value_data) {
        free(key_data);
        free(value_data);
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_IO;
    }
    
    // 读取键
    offset += sizeof(header);
    bytes = pread(file->fd, key_data, header.key_size, offset);
    if (bytes != header.key_size) {
        free(key_data);
        free(value_data);
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_IO;
    }
    
    // 读取值
    offset += header.key_size;
    bytes = pread(file->fd, value_data, header.value_size, offset);
    if (bytes != header.value_size) {
        free(key_data);
        free(value_data);
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_IO;
    }
    
    // 验证校验和
    uint32_t checksum = calculate_checksum(key_data, header.key_size);
    checksum = calculate_checksum(value_data, header.value_size);
    if (checksum != header.checksum) {
        free(key_data);
        free(value_data);
        pthread_mutex_unlock(&file->lock);
        return KVDB_ERR_CORRUPT;
    }
    
    pthread_mutex_unlock(&file->lock);
    
    *key = key_data;
    *key_len = header.key_size;
    *value = value_data;
    *value_len = header.value_size;
    
    return KVDB_OK;
} 