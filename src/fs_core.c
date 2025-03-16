#include "fs.h"
#include "device.h"
#include "memory.h"
#include <string.h>

// 全局变量
static mount_point_t mount_points[MAX_MOUNT_POINTS];
static file_desc_t file_descs[MAX_OPEN_FILES];
static cache_block_t *cache_head = NULL;
static uint32_t cache_size = 0;
static const uint32_t MAX_CACHE_BLOCKS = 1024;

// 初始化文件系统
int fs_init(void) {
    // 初始化挂载点数组
    memset(mount_points, 0, sizeof(mount_points));
    
    // 初始化文件描述符数组
    memset(file_descs, 0, sizeof(file_descs));
    
    // 初始化缓存
    cache_head = NULL;
    cache_size = 0;
    
    return 0;
}

// 查找空闲文件描述符
static int find_free_fd(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (file_descs[i].count == 0) {
            return i;
        }
    }
    return -1;
}

// 查找挂载点
static mount_point_t *find_mount_point(const char *path) {
    int longest_match = 0;
    mount_point_t *found = NULL;
    
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (mount_points[i].mounted) {
            int len = strlen(mount_points[i].mount_point);
            if (strncmp(path, mount_points[i].mount_point, len) == 0 &&
                len > longest_match) {
                longest_match = len;
                found = &mount_points[i];
            }
        }
    }
    
    return found;
}

// 读取块数据
static int read_block(mount_point_t *mp, uint32_t block_no, void *buffer) {
    // 首先查找缓存
    cache_block_t *cache = cache_get_block(block_no);
    if (cache) {
        memcpy(buffer, cache->data, BLOCK_SIZE);
        return 0;
    }
    
    // 从设备读取
    if (device_read(mp->device, block_no * BLOCK_SIZE, buffer, BLOCK_SIZE) != BLOCK_SIZE) {
        return -1;
    }
    
    // 添加到缓存
    cache = malloc(sizeof(cache_block_t));
    cache->block_no = block_no;
    cache->data = malloc(BLOCK_SIZE);
    memcpy(cache->data, buffer, BLOCK_SIZE);
    cache->dirty = false;
    cache->ref_count = 1;
    
    // 插入缓存链表
    if (cache_head) {
        cache->next = cache_head;
        cache->prev = cache_head->prev;
        cache_head->prev->next = cache;
        cache_head->prev = cache;
    } else {
        cache_head = cache;
        cache->next = cache;
        cache->prev = cache;
    }
    
    cache_size++;
    
    // 如果缓存太大，释放最旧的块
    while (cache_size > MAX_CACHE_BLOCKS) {
        cache_block_t *oldest = cache_head->prev;
        if (oldest->dirty) {
            device_write(mp->device, oldest->block_no * BLOCK_SIZE,
                        oldest->data, BLOCK_SIZE);
        }
        oldest->prev->next = oldest->next;
        oldest->next->prev = oldest->prev;
        free(oldest->data);
        free(oldest);
        cache_size--;
    }
    
    return 0;
}

// 写入块数据
static int write_block(mount_point_t *mp, uint32_t block_no, const void *buffer) {
    // 首先查找缓存
    cache_block_t *cache = cache_get_block(block_no);
    if (cache) {
        memcpy(cache->data, buffer, BLOCK_SIZE);
        cache->dirty = true;
        return 0;
    }
    
    // 添加到缓存
    cache = malloc(sizeof(cache_block_t));
    cache->block_no = block_no;
    cache->data = malloc(BLOCK_SIZE);
    memcpy(cache->data, buffer, BLOCK_SIZE);
    cache->dirty = true;
    cache->ref_count = 1;
    
    // 插入缓存链表
    if (cache_head) {
        cache->next = cache_head;
        cache->prev = cache_head->prev;
        cache_head->prev->next = cache;
        cache_head->prev = cache;
    } else {
        cache_head = cache;
        cache->next = cache;
        cache->prev = cache;
    }
    
    cache_size++;
    
    return 0;
}

// 分配新块
static uint32_t alloc_block(mount_point_t *mp) {
    superblock_t *sb = mp->sb;
    
    if (sb->free_blocks == 0) {
        return 0;
    }
    
    // 查找空闲块
    uint8_t bitmap[BLOCK_SIZE];
    uint32_t bitmap_block = sb->first_data_block + 1;
    
    for (uint32_t group = 0; group < sb->block_group_nr; group++) {
        read_block(mp, bitmap_block + group, bitmap);
        
        for (uint32_t bit = 0; bit < sb->blocks_per_group; bit++) {
            if ((bitmap[bit / 8] & (1 << (bit % 8))) == 0) {
                // 找到空闲块
                bitmap[bit / 8] |= (1 << (bit % 8));
                write_block(mp, bitmap_block + group, bitmap);
                
                sb->free_blocks--;
                write_block(mp, 0, sb);
                
                return group * sb->blocks_per_group + bit;
            }
        }
    }
    
    return 0;
}

// 释放块
static void free_block(mount_point_t *mp, uint32_t block_no) {
    superblock_t *sb = mp->sb;
    
    uint32_t group = block_no / sb->blocks_per_group;
    uint32_t bit = block_no % sb->blocks_per_group;
    
    uint8_t bitmap[BLOCK_SIZE];
    uint32_t bitmap_block = sb->first_data_block + 1 + group;
    
    read_block(mp, bitmap_block, bitmap);
    bitmap[bit / 8] &= ~(1 << (bit % 8));
    write_block(mp, bitmap_block, bitmap);
    
    sb->free_blocks++;
    write_block(mp, 0, sb);
}

// 读取inode
static int read_inode(mount_point_t *mp, uint32_t inode_no, inode_t *inode) {
    superblock_t *sb = mp->sb;
    
    uint32_t group = (inode_no - 1) / sb->inodes_per_group;
    uint32_t index = (inode_no - 1) % sb->inodes_per_group;
    
    uint32_t inode_table = sb->first_inode + group * sb->inodes_per_group;
    uint32_t block = inode_table + index * sb->inode_size / BLOCK_SIZE;
    uint32_t offset = (index * sb->inode_size) % BLOCK_SIZE;
    
    uint8_t buffer[BLOCK_SIZE];
    if (read_block(mp, block, buffer) < 0) {
        return -1;
    }
    
    memcpy(inode, buffer + offset, sizeof(inode_t));
    return 0;
}

// 写入inode
static int write_inode(mount_point_t *mp, uint32_t inode_no, const inode_t *inode) {
    superblock_t *sb = mp->sb;
    
    uint32_t group = (inode_no - 1) / sb->inodes_per_group;
    uint32_t index = (inode_no - 1) % sb->inodes_per_group;
    
    uint32_t inode_table = sb->first_inode + group * sb->inodes_per_group;
    uint32_t block = inode_table + index * sb->inode_size / BLOCK_SIZE;
    uint32_t offset = (index * sb->inode_size) % BLOCK_SIZE;
    
    uint8_t buffer[BLOCK_SIZE];
    if (read_block(mp, block, buffer) < 0) {
        return -1;
    }
    
    memcpy(buffer + offset, inode, sizeof(inode_t));
    
    if (write_block(mp, block, buffer) < 0) {
        return -1;
    }
    
    return 0;
} 