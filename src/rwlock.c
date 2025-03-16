#include "sync.h"
#include "interrupt.h"
#include "task.h"

// 初始化读写锁
void rwlock_init(rwlock_t *rwlock, const char *name) {
    if (!rwlock) return;
    
    mutex_init(&rwlock->mutex, "rwlock_mutex");
    condition_init(&rwlock->readers, "rwlock_readers");
    condition_init(&rwlock->writers, "rwlock_writers");
    rwlock->readers_count = 0;
    rwlock->writer_active = false;
    rwlock->writer_owner = NULL;
    rwlock->name = name;
}

// 获取读锁
void rwlock_read_lock(rwlock_t *rwlock) {
    if (!rwlock) return;
    
    mutex_lock(&rwlock->mutex);
    
    // 等待直到没有写者
    while (rwlock->writer_active) {
        sync_stats.rwlock_contentions++;
        condition_wait(&rwlock->readers, &rwlock->mutex);
    }
    
    rwlock->readers_count++;
    mutex_unlock(&rwlock->mutex);
}

// 尝试获取读锁
bool rwlock_read_trylock(rwlock_t *rwlock) {
    if (!rwlock) return false;
    
    if (!mutex_trylock(&rwlock->mutex)) {
        return false;
    }
    
    if (rwlock->writer_active) {
        mutex_unlock(&rwlock->mutex);
        return false;
    }
    
    rwlock->readers_count++;
    mutex_unlock(&rwlock->mutex);
    return true;
}

// 释放读锁
void rwlock_read_unlock(rwlock_t *rwlock) {
    if (!rwlock) return;
    
    mutex_lock(&rwlock->mutex);
    
    if (--rwlock->readers_count == 0) {
        condition_signal(&rwlock->writers);
    }
    
    mutex_unlock(&rwlock->mutex);
}

// 获取写锁
void rwlock_write_lock(rwlock_t *rwlock) {
    if (!rwlock) return;
    
    mutex_lock(&rwlock->mutex);
    
    // 等待直到没有读者和写者
    while (rwlock->readers_count > 0 || rwlock->writer_active) {
        sync_stats.rwlock_contentions++;
        condition_wait(&rwlock->writers, &rwlock->mutex);
    }
    
    rwlock->writer_active = true;
    rwlock->writer_owner = task_get_current();
    mutex_unlock(&rwlock->mutex);
}

// 尝试获取写锁
bool rwlock_write_trylock(rwlock_t *rwlock) {
    if (!rwlock) return false;
    
    if (!mutex_trylock(&rwlock->mutex)) {
        return false;
    }
    
    if (rwlock->readers_count > 0 || rwlock->writer_active) {
        mutex_unlock(&rwlock->mutex);
        return false;
    }
    
    rwlock->writer_active = true;
    rwlock->writer_owner = task_get_current();
    mutex_unlock(&rwlock->mutex);
    return true;
}

// 释放写锁
void rwlock_write_unlock(rwlock_t *rwlock) {
    if (!rwlock) return;
    
    mutex_lock(&rwlock->mutex);
    
    if (rwlock->writer_owner == task_get_current()) {
        rwlock->writer_active = false;
        rwlock->writer_owner = NULL;
        
        // 优先唤醒写者，然后是读者
        condition_signal(&rwlock->writers);
        condition_broadcast(&rwlock->readers);
    }
    
    mutex_unlock(&rwlock->mutex);
} 
// 互斥量示例
mutex_t mutex;
mutex_init(&mutex, "test_mutex");

void task1(void) {
    while (1) {
        mutex_lock(&mutex);
        // 临界区代码
        mutex_unlock(&mutex);
        task_sleep(100);
    }
}

// 信号量示例
semaphore_t sem;
semaphore_init(&sem, 2, "test_semaphore");

void producer(void) {
    while (1) {
        // 生产资源
        semaphore_post(&sem);
        task_sleep(100);
    }
}

void consumer(void) {
    while (1) {
        semaphore_wait(&sem);
        // 消费资源
        task_sleep(200);
    }
}

// 条件变量示例
mutex_t mutex;
condition_t cond;
bool data_ready = false;

void producer_task(void) {
    while (1) {
        mutex_lock(&mutex);
        // 准备数据
        data_ready = true;
        condition_signal(&cond);
        mutex_unlock(&mutex);
        task_sleep(100);
    }
}

void consumer_task(void) {
    while (1) {
        mutex_lock(&mutex);
        while (!data_ready) {
            condition_wait(&cond, &mutex);
        }
        // 处理数据
        data_ready = false;
        mutex_unlock(&mutex);
    }
}

// 读写锁示例
rwlock_t rwlock;
rwlock_init(&rwlock, "test_rwlock");

void reader_task(void) {
    while (1) {
        rwlock_read_lock(&rwlock);
        // 读取数据
        rwlock_read_unlock(&rwlock);
        task_sleep(100);
    }
}

void writer_task(void) {
    while (1) {
        rwlock_write_lock(&rwlock);
        // 修改数据
        rwlock_write_unlock(&rwlock);
        task_sleep(200);
    }
}