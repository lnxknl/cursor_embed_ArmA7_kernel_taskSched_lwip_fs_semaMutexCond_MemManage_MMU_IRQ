#include "ipc.h"
#include "task.h"
#include "memory.h"

// IPC 系统初始化
void ipc_init(void) {
    // 初始化消息队列系统
    msgq_init();
    
    // 初始化共享内存系统
    shm_init();
    
    // 初始化管道系统
    pipe_init();
}

// 获取 IPC 统计信息
int ipc_get_stats(ipc_stats_t *stats) {
    if (!stats) return -1;
    memcpy(stats, &ipc_stats, sizeof(ipc_stats_t));
    return 0;
}

// 重置 IPC 统计信息
void ipc_reset_stats(void) {
    memset(&ipc_stats, 0, sizeof(ipc_stats_t));
} 