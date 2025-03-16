#include "ipc.h"
#include "memory.h"
#include <string.h>

#define PIPE_BUF_SIZE 4096

// 全局管道链表
static pipe_t *pipes = NULL;
static mutex_t pipes_lock;

// 初始化管道系统
void pipe_init(void) {
    mutex_init(&pipes_lock, "pipes_lock");
}

// 创建管道
int pipe_create(int pipefd[2]) {
    mutex_lock(&pipes_lock);

    // 分配管道结构
    pipe_t *pipe = malloc(sizeof(pipe_t));
    if (!pipe) {
        mutex_unlock(&pipes_lock);
        return -1;
    }

    // 分配缓冲区
    pipe->buffer = malloc(PIPE_BUF_SIZE);
    if (!pipe->buffer) {
        free(pipe);
        mutex_unlock(&pipes_lock);
        return -1;
    }

    // 初始化管道
    pipe->size = PIPE_BUF_SIZE;
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->count = 0;
    mutex_init(&pipe->lock, "pipe_lock");
    condition_init(&pipe->not_full, "pipe_not_full");
    condition_init(&pipe->not_empty, "pipe_not_empty");
    pipe->reader_closed = false;
    pipe->writer_closed = false;

    // 分配文件描述符
    int read_fd = fd_alloc();
    int write_fd = fd_alloc();
    if (read_fd < 0 || write_fd < 0) {
        free(pipe->buffer);
        free(pipe);
        mutex_unlock(&pipes_lock);
        return -1;
    }

    // 设置文件描述符
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;

    // 添加到链表
    pipe->next = pipes;
    pipes = pipe;
    ipc_stats.pipes++;

    mutex_unlock(&pipes_lock);
    return 0;
}

// 写入管道
int pipe_write(int fd, const void *buf, uint32_t count) {
    pipe_t *pipe = find_pipe_by_fd(fd);
    if (!pipe || pipe->writer_closed) return -1;

    mutex_lock(&pipe->lock);

    uint32_t written = 0;
    const uint8_t *src = buf;

    while (written < count) {
        // 等待直到有空间可写
        while (pipe->count == pipe->size) {
            if (pipe->reader_closed) {
                mutex_unlock(&pipe->lock);
                return -1;
            }
            condition_wait(&pipe->not_full, &pipe->lock);
        }

        // 计算可写入的数量
        uint32_t space = pipe->size - pipe->count;
        uint32_t to_write = count - written;
        if (to_write > space) to_write = space;

        // 写入数据
        uint32_t first_part = pipe->size - pipe->write_pos;
        if (to_write <= first_part) {
            memcpy(pipe->buffer + pipe->write_pos, src + written, to_write);
            pipe->write_pos += to_write;
            if (pipe->write_pos == pipe->size) pipe->write_pos = 0;
        } else {
            memcpy(pipe->buffer + pipe->write_pos, src + written, first_part);
            memcpy(pipe->buffer, src + written + first_part, to_write - first_part);
            pipe->write_pos = to_write - first_part;
        }

        pipe->count += to_write;
        written += to_write;
        ipc_stats.pipe_writes++;

        // 通知等待的读者
        condition_signal(&pipe->not_empty);
    }

    mutex_unlock(&pipe->lock);
    return written;
}

// 从管道读取
int pipe_read(int fd, void *buf, uint32_t count) {
    pipe_t *pipe = find_pipe_by_fd(fd);
    if (!pipe || pipe->reader_closed) return -1;

    mutex_lock(&pipe->lock);

    uint32_t read = 0;
    uint8_t *dst = buf;

    while (read < count) {
        // 等待直到有数据可读
        while (pipe->count == 0) {
            if (pipe->writer_closed) {
                if (read > 0) {
                    mutex_unlock(&pipe->lock);
                    return read;
                }
                mutex_unlock(&pipe->lock);
                return 0;
            }
            condition_wait(&pipe->not_empty, &pipe->lock);
        }

        // 计算可读取的数量
        uint32_t available = pipe->count;
        uint32_t to_read = count - read;
        if (to_read > available) to_read = available;

        // 读取数据
        uint32_t first_part = pipe->size - pipe->read_pos;
        if (to_read <= first_part) {
            memcpy(dst + read, pipe->buffer + pipe->read_pos, to_read);
            pipe->read_pos += to_read;
            if (pipe->read_pos == pipe->size) pipe->read_pos = 0;
        } else {
            memcpy(dst + read, pipe->buffer + pipe->read_pos, first_part);
            memcpy(dst + read + first_part, pipe->buffer, to_read - first_part);
            pipe->read_pos = to_read - first_part;
        }

        pipe->count -= to_read;
        read += to_read;
        ipc_stats.pipe_reads++;

        // 通知等待的写者
        condition_signal(&pipe->not_full);
    }

    mutex_unlock(&pipe->lock);
    return read;
} 