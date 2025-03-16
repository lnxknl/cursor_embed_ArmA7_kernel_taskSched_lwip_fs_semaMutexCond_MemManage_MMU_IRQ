#include "mm.h"
#include "sync.h"
#include <string.h>

// 页面框架描述符
typedef struct page_frame {
    uint32_t vaddr;            // 虚拟地址
    uint32_t accessed;         // 访问位
    uint32_t dirty;            // 脏位
    uint32_t reference;        // 引用计数
    struct page_frame *next;   // 链表指针
} page_frame_t;

// 页面替换算法上下文
typedef struct {
    pra_type_t type;          // 算法类型
    page_frame_t *frames;     // 页面框架数组
    uint32_t frame_count;     // 页面框架数量
    uint32_t clock_hand;      // 时钟算法指针
    pra_stats_t stats;        // 统计信息
    mutex_t lock;             // 互斥锁
} pra_context_t;

static pra_context_t pra_ctx;

// 初始化页面替换算法
void pra_init(pra_type_t type) {
    memset(&pra_ctx, 0, sizeof(pra_ctx));
    pra_ctx.type = type;
    mutex_init(&pra_ctx.lock, "pra_lock");
    
    // 分配页面框架数组
    pra_ctx.frame_count = 1024; // 4MB物理内存
    pra_ctx.frames = mm_alloc(sizeof(page_frame_t) * pra_ctx.frame_count);
    memset(pra_ctx.frames, 0, sizeof(page_frame_t) * pra_ctx.frame_count);
    
    // 初始化链表
    for (uint32_t i = 0; i < pra_ctx.frame_count - 1; i++) {
        pra_ctx.frames[i].next = &pra_ctx.frames[i + 1];
    }
}

// FIFO算法选择被替换页面
static page_frame_t *fifo_select(void) {
    static page_frame_t *fifo_head = NULL;
    
    if (!fifo_head) {
        fifo_head = pra_ctx.frames;
    }
    
    page_frame_t *victim = fifo_head;
    fifo_head = fifo_head->next;
    return victim;
}

// 时钟算法选择被替换页面
static page_frame_t *clock_select(void) {
    while (1) {
        page_frame_t *frame = &pra_ctx.frames[pra_ctx.clock_hand];
        pra_ctx.clock_hand = (pra_ctx.clock_hand + 1) % pra_ctx.frame_count;
        
        if (frame->accessed) {
            frame->accessed = 0;
        } else {
            return frame;
        }
    }
}

// LRU算法选择被替换页面
static page_frame_t *lru_select(void) {
    page_frame_t *victim = NULL;
    uint32_t min_ref = UINT32_MAX;
    
    for (uint32_t i = 0; i < pra_ctx.frame_count; i++) {
        if (pra_ctx.frames[i].reference < min_ref) {
            min_ref = pra_ctx.frames[i].reference;
            victim = &pra_ctx.frames[i];
        }
    }
    
    return victim;
}

// NFU算法选择被替换页面
static page_frame_t *nfu_select(void) {
    page_frame_t *victim = NULL;
    uint32_t min_ref = UINT32_MAX;
    
    for (uint32_t i = 0; i < pra_ctx.frame_count; i++) {
        uint32_t ref = pra_ctx.frames[i].reference >> 1; // 老化
        pra_ctx.frames[i].reference = ref;
        
        if (ref < min_ref) {
            min_ref = ref;
            victim = &pra_ctx.frames[i];
        }
    }
    
    return victim;
}

// 分配页面
void *pra_alloc_page(void) {
    mutex_lock(&pra_ctx.lock);
    
    // 查找空闲页面
    page_frame_t *frame = NULL;
    for (uint32_t i = 0; i < pra_ctx.frame_count; i++) {
        if (pra_ctx.frames[i].vaddr == 0) {
            frame = &pra_ctx.frames[i];
            break;
        }
    }
    
    if (!frame) {
        // 没有空闲页面，需要替换
        pra_ctx.stats.page_faults++;
        
        // 选择被替换页面
        switch (pra_ctx.type) {
            case PRA_FIFO:
                frame = fifo_select();
                break;
            case PRA_CLOCK:
                frame = clock_select();
                break;
            case PRA_LRU:
                frame = lru_select();
                break;
            case PRA_NFU:
                frame = nfu_select();
                break;
        }
        
        if (frame->dirty) {
            // 写回脏页面
            void *swap_addr = swap_out(frame->vaddr);
            if (!swap_addr) {
                mutex_unlock(&pra_ctx.lock);
                return NULL;
            }
            pra_ctx.stats.page_outs++;
        }
        
        // 解除原映射
        mmu_unmap_page(frame->vaddr);
        pra_ctx.stats.replaced_pages++;
    }
    
    // 分配新的物理页面
    void *page = mm_alloc_pages(1);
    if (!page) {
        mutex_unlock(&pra_ctx.lock);
        return NULL;
    }
    
    // 初始化页面框架
    frame->vaddr = (uint32_t)page;
    frame->accessed = 1;
    frame->dirty = 0;
    frame->reference = 0;
    
    pra_ctx.stats.page_ins++;
    
    mutex_unlock(&pra_ctx.lock);
    return page;
}

// 释放页面
void pra_free_page(void *addr) {
    mutex_lock(&pra_ctx.lock);
    
    // 查找页面框架
    for (uint32_t i = 0; i < pra_ctx.frame_count; i++) {
        if (pra_ctx.frames[i].vaddr == (uint32_t)addr) {
            // 清除页面框架
            pra_ctx.frames[i].vaddr = 0;
            pra_ctx.frames[i].accessed = 0;
            pra_ctx.frames[i].dirty = 0;
            pra_ctx.frames[i].reference = 0;
            
            // 释放物理页面
            mm_free_pages(addr, 1);
            break;
        }
    }
    
    mutex_unlock(&pra_ctx.lock);
}

// 页面访问
void pra_access_page(void *addr) {
    mutex_lock(&pra_ctx.lock);
    
    // 查找页面框架
    for (uint32_t i = 0; i < pra_ctx.frame_count; i++) {
        if (pra_ctx.frames[i].vaddr == (uint32_t)addr) {
            // 更新访问信息
            pra_ctx.frames[i].accessed = 1;
            pra_ctx.frames[i].reference++;
            
            if (pra_ctx.type == PRA_NFU) {
                pra_ctx.frames[i].reference |= 0x80000000; // 设置最高位
            }
            break;
        }
    }
    
    mutex_unlock(&pra_ctx.lock);
} 