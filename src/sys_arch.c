#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/stats.h"
#include "arch/sys_arch.h"
#include "os.h"

// 系统时钟
static uint32_t sys_now_ms = 0;

// 初始化系统架构
void sys_init(void)
{
    // 初始化系统时钟
    sys_now_ms = 0;
}

// 创建信号量
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    *sem = os_sem_create(count);
    if (*sem == NULL) {
        SYS_STATS_INC(sem.err);
        return ERR_MEM;
    }
    SYS_STATS_INC_USED(sem);
    return ERR_OK;
}

// 释放信号量
void sys_sem_free(sys_sem_t *sem)
{
    os_sem_delete(*sem);
    SYS_STATS_DEC(sem.used);
}

// 发送信号量
void sys_sem_signal(sys_sem_t *sem)
{
    os_sem_give(*sem);
}

// 等待信号量
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    uint32_t start = sys_now();
    
    if (timeout == 0) {
        os_sem_take(*sem, OS_WAIT_FOREVER);
        return (sys_now() - start);
    }

    if (os_sem_take(*sem, timeout) == OS_TIMEOUT) {
        return SYS_ARCH_TIMEOUT;
    }

    return (sys_now() - start);
}

// 创建互斥量
err_t sys_mutex_new(sys_mutex_t *mutex)
{
    *mutex = os_mutex_create();
    if (*mutex == NULL) {
        SYS_STATS_INC(mutex.err);
        return ERR_MEM;
    }
    SYS_STATS_INC_USED(mutex);
    return ERR_OK;
}

// 释放互斥量
void sys_mutex_free(sys_mutex_t *mutex)
{
    os_mutex_delete(*mutex);
    SYS_STATS_DEC(mutex.used);
}

// 锁定互斥量
void sys_mutex_lock(sys_mutex_t *mutex)
{
    os_mutex_take(*mutex, OS_WAIT_FOREVER);
}

// 解锁互斥量
void sys_mutex_unlock(sys_mutex_t *mutex)
{
    os_mutex_give(*mutex);
}

// 创建邮箱
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    *mbox = os_queue_create(size, sizeof(void *));
    if (*mbox == NULL) {
        SYS_STATS_INC(mbox.err);
        return ERR_MEM;
    }
    SYS_STATS_INC_USED(mbox);
    return ERR_OK;
}

// 释放邮箱
void sys_mbox_free(sys_mbox_t *mbox)
{
    os_queue_delete(*mbox);
    SYS_STATS_DEC(mbox.used);
}

// 发送邮箱消息
void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    os_queue_send(*mbox, &msg, OS_WAIT_FOREVER);
}

// 尝试发送邮箱消息
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (os_queue_send(*mbox, &msg, 0) != OS_OK) {
        SYS_STATS_INC(mbox.err);
        return ERR_MEM;
    }
    return ERR_OK;
}

// 接收邮箱消息
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    uint32_t start = sys_now();
    void *dummy;

    if (msg == NULL) {
        msg = &dummy;
    }

    if (timeout == 0) {
        os_queue_receive(*mbox, msg, OS_WAIT_FOREVER);
        return (sys_now() - start);
    }

    if (os_queue_receive(*mbox, msg, timeout) == OS_TIMEOUT) {
        *msg = NULL;
        return SYS_ARCH_TIMEOUT;
    }

    return (sys_now() - start);
}

// 尝试接收邮箱消息
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *dummy;

    if (msg == NULL) {
        msg = &dummy;
    }

    if (os_queue_receive(*mbox, msg, 0) == OS_TIMEOUT) {
        *msg = NULL;
        return SYS_ARCH_TIMEOUT;
    }

    return 0;
}

// 创建线程
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread,
                           void *arg, int stacksize, int prio)
{
    os_task_t task;
    
    if (os_task_create(&task, name, thread, arg, prio, stacksize) != OS_OK) {
        return NULL;
    }
    
    return task;
}

// 获取系统时间
u32_t sys_now(void)
{
    return sys_now_ms;
}

// 系统时钟中断处理
void sys_tick_handler(void)
{
    sys_now_ms++;
} 