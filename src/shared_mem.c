#include "ipc.h"
#include "memory.h"
#include "mmu.h"
#include <string.h>

// 全局共享内存段链表
static shm_segment_t *shm_segments = NULL;
static mutex_t shm_segments_lock;

// 初始化共享内存系统
void shm_init(void) {
    mutex_init(&shm_segments_lock, "shm_segments_lock");
}

// 查找共享内存段
static shm_segment_t *find_shm_segment(key_t key) {
    shm_segment_t *seg = shm_segments;
    while (seg) {
        if (seg->key == key) {
            return seg;
        }
        seg = seg->next;
    }
    return NULL;
}

// 创建共享内存段
int shm_create(key_t key, uint32_t size) {
    mutex_lock(&shm_segments_lock);

    // 检查是否已存在
    if (find_shm_segment(key)) {
        mutex_unlock(&shm_segments_lock);
        return -1;
    }

    // 分配共享内存段结构
    shm_segment_t *seg = malloc(sizeof(shm_segment_t));
    if (!seg) {
        mutex_unlock(&shm_segments_lock);
        return -1;
    }

    // 分配物理内存
    void *phys_addr = mm_alloc_pages(size / PAGE_SIZE + 1);
    if (!phys_addr) {
        free(seg);
        mutex_unlock(&shm_segments_lock);
        return -1;
    }

    // 初始化共享内存段
    seg->key = key;
    seg->size = size;
    seg->phys_addr = phys_addr;
    seg->ref_count = 0;
    mutex_init(&seg->lock, "shm_lock");

    // 添加到链表
    seg->next = shm_segments;
    shm_segments = seg;
    ipc_stats.shm_segments++;

    mutex_unlock(&shm_segments_lock);
    return 0;
}

// 附加共享内存段
void *shm_attach(int shmid) {
    shm_segment_t *seg = find_shm_segment(shmid);
    if (!seg) return NULL;

    mutex_lock(&seg->lock);

    // 为当前任务分配虚拟地址空间
    void *virt_addr = mm_alloc_virt_pages(seg->size / PAGE_SIZE + 1);
    if (!virt_addr) {
        mutex_unlock(&seg->lock);
        return NULL;
    }

    // 建立映射
    for (uint32_t offset = 0; offset < seg->size; offset += PAGE_SIZE) {
        mmu_map_page((uint32_t)virt_addr + offset,
                     (uint32_t)seg->phys_addr + offset,
                     MMU_PERM_READ | MMU_PERM_WRITE | MMU_PERM_USER);
    }

    seg->ref_count++;
    ipc_stats.shm_attaches++;

    mutex_unlock(&seg->lock);
    return virt_addr;
}

// 分离共享内存段
int shm_detach(void *addr) {
    mutex_lock(&shm_segments_lock);

    // 查找对应的共享内存段
    shm_segment_t *seg = shm_segments;
    while (seg) {
        task_t *current = task_get_current();
        if (current->mm->shm_areas && 
            current->mm->shm_areas->addr == addr) {
            break;
        }
        seg = seg->next;
    }

    if (!seg) {
        mutex_unlock(&shm_segments_lock);
        return -1;
    }

    mutex_lock(&seg->lock);

    // 解除映射
    for (uint32_t offset = 0; offset < seg->size; offset += PAGE_SIZE) {
        mmu_unmap_page((uint32_t)addr + offset);
    }

    // 释放虚拟地址空间
    mm_free_virt_pages(addr, seg->size / PAGE_SIZE + 1);

    seg->ref_count--;

    mutex_unlock(&seg->lock);
    mutex_unlock(&shm_segments_lock);
    return 0;
} 