#include "fs.h"
#include <string.h>

// 打开文件
int fs_open(const char *path, int flags) {
    // 查找挂载点
    mount_point_t *mp = find_mount_point(path);
    if (!mp) {
        return -1;
    }
    
    // 解析路径
    char local_path[MAX_PATH];
    strncpy(local_path, path + strlen(mp->mount_point), MAX_PATH);
    
    // 查找inode
    uint32_t inode_no;
    if (lookup_path(mp, local_path, &inode_no) < 0) {
        if (!(flags & O_CREAT)) {
            return -1;
        }
        
        // 创建新文件
        inode_no = create_file(mp, local_path, FT_REGULAR);
        if (inode_no == 0) {
            return -1;
        }
    }
    
    // 分配文件描述符
    int fd = find_free_fd();
    if (fd < 0) {
        return -1;
    }
    
    file_descs[fd].inode = inode_no;
    file_descs[fd].pos = 0;
    file_descs[fd].flags = flags;
    file_descs[fd].count = 1;
    
    return fd;
}

// 关闭文件
int fs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }
    
    if (file_descs[fd].count == 0) {
        return -1;
    }
    
    file_descs[fd].count--;
    if (file_descs[fd].count == 0) {
        // 文件完全关闭，同步缓存
        cache_sync();
    }
    
    return 0;
}

// 读取文件数据
int fs_read(int fd, void *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }
    
    file_desc_t *desc = &file_descs[fd];
    if (desc->count == 0) {
        return -1;
    }
    
    // 查找挂载点
    mount_point_t *mp = find_mount_point_by_inode(desc->inode);
    if (!mp) {
        return -1;
    }
    
    // 读取inode
    inode_t inode;
    if (read_inode(mp, desc->inode, &inode) < 0) {
        return -1;
    }
    
    // 检查是否超出文件末尾
    if (desc->pos >= inode.size) {
        return 0;
    }
    
    // 调整读取大小
    if (desc->pos + count > inode.size) {
        count = inode.size - desc->pos;
    }
    
    uint32_t bytes_read = 0;
    uint8_t *dst = buf;
    
    while (bytes_read < count) {
        // 计算块号和偏移
        uint32_t block_no = desc->pos / BLOCK_SIZE;
        uint32_t offset = desc->pos % BLOCK_SIZE;
        
        // 获取数据块号
        uint32_t data_block;
        if (block_no < 12) {
            data_block = inode.direct[block_no];
        } else if (block_no < 12 + BLOCK_SIZE/4) {
            // 一级间接块
            uint32_t indirect[BLOCK_SIZE/4];
            read_block(mp, inode.indirect, indirect);
            data_block = indirect[block_no - 12];
        } else if (block_no < 12 + BLOCK_SIZE/4 + (BLOCK_SIZE/4)*(BLOCK_SIZE/4)) {
            // 二级间接块
            uint32_t double_indirect[BLOCK_SIZE/4];
            read_block(mp, inode.double_indirect, double_indirect);
            
            uint32_t index1 = (block_no - 12 - BLOCK_SIZE/4) / (BLOCK_SIZE/4);
            uint32_t index2 = (block_no - 12 - BLOCK_SIZE/4) % (BLOCK_SIZE/4);
            
            uint32_t indirect[BLOCK_SIZE/4];
            read_block(mp, double_indirect[index1], indirect);
            data_block = indirect[index2];
        } else {
            // 三级间接块
            // ... 类似实现
        }
        
        // 读取数据块
        uint8_t block[BLOCK_SIZE];
        read_block(mp, data_block, block);
        
        // 复制数据
        uint32_t chunk = BLOCK_SIZE - offset;
        if (chunk > count - bytes_read) {
            chunk = count - bytes_read;
        }
        
        memcpy(dst + bytes_read, block + offset, chunk);
        bytes_read += chunk;
        desc->pos += chunk;
    }
    
    return bytes_read;
}

// 写入文件数据
int fs_write(int fd, const void *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }
    
    file_desc_t *desc = &file_descs[fd];
    if (desc->count == 0) {
        return -1;
    }
    
    // 查找挂载点
    mount_point_t *mp = find_mount_point_by_inode(desc->inode);
    if (!mp) {
        return -1;
    }
    
    // 读取inode
    inode_t inode;
    if (read_inode(mp, desc->inode, &inode) < 0) {
        return -1;
    }
    
    uint32_t bytes_written = 0;
    const uint8_t *src = buf;
    
    while (bytes_written < count) {
        // 计算块号和偏移
        uint32_t block_no = desc->pos / BLOCK_SIZE;
        uint32_t offset = desc->pos % BLOCK_SIZE;
        
        // 获取或分配数据块
        uint32_t data_block;
        if (block_no < 12) {
            if (inode.direct[block_no] == 0) {
                inode.direct[block_no] = alloc_block(mp);
                if (inode.direct[block_no] == 0) {
                    break;
                }
            }
            data_block = inode.direct[block_no];
        } else if (block_no < 12 + BLOCK_SIZE/4) {
            // 一级间接块
            if (inode.indirect == 0) {
                inode.indirect = alloc_block(mp);
                if (inode.indirect == 0) {
                    break;
                }
            }
            
            uint32_t indirect[BLOCK_SIZE/4];
            read_block(mp, inode.indirect, indirect);
            
            if (indirect[block_no - 12] == 0) {
                indirect[block_no - 12] = alloc_block(mp);
                if (indirect[block_no - 12] == 0) {
                    break;
                }
                write_block(mp, inode.indirect, indirect);
            }
            
            data_block = indirect[block_no - 12];
        } else {
            // 二级和三级间接块类似实现
        }
        
        // 读取数据块
        uint8_t block[BLOCK_SIZE];
        if (offset > 0 || bytes_written + BLOCK_SIZE > count) {
            read_block(mp, data_block, block);
        }
        
        // 写入数据
        uint32_t chunk = BLOCK_SIZE - offset;
        if (chunk > count - bytes_written) {
            chunk = count - bytes_written;
        }
        
        memcpy(block + offset, src + bytes_written, chunk);
        write_block(mp, data_block, block);
        
        bytes_written += chunk;
        desc->pos += chunk;
        
        // 更新文件大小
        if (desc->pos > inode.size) {
            inode.size = desc->pos;
        }
    }
    
    // 更新inode
    inode.mtime = get_time();
    write_inode(mp, desc->inode, &inode);
    
    return bytes_written;
} 

// 文件系统使用示例

void fs_test(void) {
    // 1. 初始化文件系统
    fs_init();
    
    // 2. 挂载文件系统
    fs_mount("/dev/disk0", "/");
    
    // 3. 创建目录
    fs_mkdir("/test", 0755);
    
    // 4. 创建和写入文件
    int fd = fs_open("/test/hello.txt", O_CREAT | O_WRONLY);
    if (fd >= 0) {
        const char *message = "Hello, Embedded File System!";
        fs_write(fd, message, strlen(message));
        fs_close(fd);
    }
    
    // 5. 读取文件
    fd = fs_open("/test/hello.txt", O_RDONLY);
    if (fd >= 0) {
        char buffer[100];
        int bytes = fs_read(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("Read from file: %s\n", buffer);
        }
        fs_close(fd);
    }
    
    // 6. 目录遍历
    DIR *dir = fs_opendir("/test");
    if (dir) {
        struct dirent *entry;
        while ((entry = fs_readdir(dir)) != NULL) {
            printf("Found file: %s\n", entry->d_name);
        }
        fs_closedir(dir);
    }
    
    // 7. 文件重命名
    fs_rename("/test/hello.txt", "/test/world.txt");
    
    // 8. 删除文件
    fs_unlink("/test/world.txt");
    
    // 9. 删除目录
    fs_rmdir("/test");
    
    // 10. 卸载文件系统
    fs_unmount("/");
}

// 实际应用场景：简单的日志系统
typedef struct {
    int fd;
    char filename[MAX_PATH];
    uint32_t max_size;
    uint32_t current_size;
} log_file_t;

log_file_t *log_open(const char *filename, uint32_t max_size) {
    log_file_t *log = malloc(sizeof(log_file_t));
    if (!log) return NULL;
    
    strncpy(log->filename, filename, MAX_PATH);
    log->max_size = max_size;
    log->current_size = 0;
    
    // 打开或创建日志文件
    log->fd = fs_open(filename, O_CREAT | O_WRONLY | O_APPEND);
    if (log->fd < 0) {
        free(log);
        return NULL;
    }
    
    // 获取当前文件大小
    struct stat st;
    if (fs_stat(filename, &st) == 0) {
        log->current_size = st.st_size;
    }
    
    return log;
}

void log_write(log_file_t *log, const char *message) {
    if (!log || log->fd < 0) return;
    
    // 检查是否需要轮转
    if (log->current_size + strlen(message) + 1 > log->max_size) {
        // 关闭当前文件
        fs_close(log->fd);
        
        // 备份旧文件
        char backup[MAX_PATH];
        snprintf(backup, MAX_PATH, "%s.old", log->filename);
        fs_rename(log->filename, backup);
        
        // 创建新文件
        log->fd = fs_open(log->filename, O_CREAT | O_WRONLY);
        log->current_size = 0;
    }
    
    // 写入时间戳
    char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", localtime(&now));
    fs_write(log->fd, timestamp, strlen(timestamp));
    
    // 写入消息
    fs_write(log->fd, message, strlen(message));
    fs_write(log->fd, "\n", 1);
    
    log->current_size += strlen(timestamp) + strlen(message) + 1;
}

void log_close(log_file_t *log) {
    if (!log) return;
    
    if (log->fd >= 0) {
        fs_close(log->fd);
    }
    
    free(log);
}

// 使用日志系统
void log_system_test(void) {
    // 创建日志文件
    log_file_t *log = log_open("/var/log/system.log", 1024*1024); // 1MB最大大小
    
    // 写入一些日志
    log_write(log, "System startup");
    log_write(log, "Initializing devices...");
    log_write(log, "Network connection established");
    log_write(log, "Starting services...");
    
    // 模拟错误
    log_write(log, "ERROR: Failed to start service XYZ");
    
    // 关闭日志
    log_close(log);
}