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

// 消息队列示例
void producer_task(void) {
    int mqid = msgq_create(1234, 10, sizeof(msg_t) + 100);
    
    while (1) {
        msg_t *msg = malloc(sizeof(msg_t) + 100);
        msg->type = 1;
        msg->size = 100;
        strcpy(msg->data, "Hello from producer!");
        
        msgq_send(mqid, msg, sizeof(msg_t) + 100, 1000);
        free(msg);
        
        task_sleep(100);
    }
}

void consumer_task(void) {
    int mqid = msgq_open(1234);
    
    while (1) {
        msg_t *msg = malloc(sizeof(msg_t) + 100);
        
        if (msgq_receive(mqid, msg, sizeof(msg_t) + 100, 1, 1000) == 0) {
            printf("Received: %s\n", msg->data);
        }
        
        free(msg);
    }
}

// 共享内存示例
void task1(void) {
    int shmid = shm_create(5678, 4096);
    void *addr = shm_attach(shmid);
    
    strcpy(addr, "Hello from task1!");
    
    task_sleep(1000);
    shm_detach(addr);
}

void task2(void) {
    int shmid = shm_open(5678);
    void *addr = shm_attach(shmid);
    
    printf("Read from shared memory: %s\n", (char *)addr);
    
    shm_detach(addr);
}

// 管道示例
void pipe_test(void) {
    int pipefd[2];
    pipe_create(pipefd);
    
    if (task_fork() == 0) {
        // 子任务
        close(pipefd[1]);  // 关闭写端
        
        char buf[100];
        int n = pipe_read(pipefd[0], buf, sizeof(buf));
        printf("Child read: %s\n", buf);
        
        close(pipefd[0]);
    } else {
        // 父任务
        close(pipefd[0]);  // 关闭读端
        
        const char *msg = "Hello through pipe!";
        pipe_write(pipefd[1], msg, strlen(msg) + 1);
        
        close(pipefd[1]);
    }
}