#include "mm.h"
#include "sync.h"
#include <string.h>

// 内存块头部
typedef struct block_header {
    uint32_t size;              // 块大小
    uint32_t magic;             // 魔数，用于检测内存越界
    bool is_free;               // 是否空闲
    struct block_header *next;  // 下一个块
    struct block_header *prev;  // 上一个块
} block_header_t;

// 内存块尾部
typedef struct block_footer {
    block_header_t *header;    // 指向块头部
} block_footer_t;

#define BLOCK_MAGIC    0xDEADBEEF
#define MIN_BLOCK_SIZE (sizeof(block_header_t) + sizeof(block_footer_t) + 16)

// 空闲块链表（按大小排序）
static block_header_t *free_list = NULL;
static mutex_t mm_lock;

// 初始化内存分配器
void mm_alloc_init(void) {
    mutex_init(&mm_lock, "mm_lock");
    
    // 初始化第一个大块
    void *initial_heap = mm_alloc_pages(1024); // 4MB初始堆
    block_header_t *initial_block = (block_header_t *)initial_heap;
    initial_block->size = 1024 * PAGE_SIZE - sizeof(block_header_t) - sizeof(block_footer_t);
    initial_block->magic = BLOCK_MAGIC;
    initial_block->is_free = true;
    initial_block->next = NULL;
    initial_block->prev = NULL;
    
    // 设置尾部
    block_footer_t *footer = (block_footer_t *)((char *)initial_block + initial_block->size + sizeof(block_header_t));
    footer->header = initial_block;
    
    // 加入空闲链表
    free_list = initial_block;
}

// 分割内存块
static void split_block(block_header_t *block, size_t size) {
    if (block->size - size >= MIN_BLOCK_SIZE) {
        // 创建新块
        block_header_t *new_block = (block_header_t *)((char *)block + sizeof(block_header_t) + size);
        new_block->size = block->size - size - sizeof(block_header_t) - sizeof(block_footer_t);
        new_block->magic = BLOCK_MAGIC;
        new_block->is_free = true;
        new_block->next = block->next;
        new_block->prev = block;
        
        // 设置新块的尾部
        block_footer_t *new_footer = (block_footer_t *)((char *)new_block + new_block->size + sizeof(block_header_t));
        new_footer->header = new_block;
        
        // 更新原块
        block->size = size;
        block->next = new_block;
        
        // 更新原块的尾部
        block_footer_t *footer = (block_footer_t *)((char *)block + block->size + sizeof(block_header_t));
        footer->header = block;
        
        // 加入空闲链表
        if (new_block->next) {
            new_block->next->prev = new_block;
        }
    }
}

// 合并相邻的空闲块
static block_header_t *coalesce(block_header_t *block) {
    // 检查并合并后一个块
    if (block->next && block->next->is_free) {
        block->size += sizeof(block_header_t) + sizeof(block_footer_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
        
        // 更新尾部
        block_footer_t *footer = (block_footer_t *)((char *)block + block->size + sizeof(block_header_t));
        footer->header = block;
    }
    
    // 检查并合并前一个块
    if (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(block_header_t) + sizeof(block_footer_t) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        block = block->prev;
        
        // 更新尾部
        block_footer_t *footer = (block_footer_t *)((char *)block + block->size + sizeof(block_header_t));
        footer->header = block;
    }
    
    return block;
}

// 分配内存
void *mm_alloc(size_t size) {
    if (size == 0) return NULL;
    
    // 对齐到8字节
    size = (size + 7) & ~7;
    
    mutex_lock(&mm_lock);
    
    // 查找合适的空闲块
    block_header_t *block = free_list;
    block_header_t *best_fit = NULL;
    
    while (block) {
        if (block->is_free && block->size >= size) {
            if (!best_fit || block->size < best_fit->size) {
                best_fit = block;
            }
        }
        block = block->next;
    }
    
    if (!best_fit) {
        // 没有合适的块，申请新页面
        size_t pages = (size + sizeof(block_header_t) + sizeof(block_footer_t) + PAGE_SIZE - 1) / PAGE_SIZE;
        void *new_mem = mm_alloc_pages(pages);
        if (!new_mem) {
            mutex_unlock(&mm_lock);
            return NULL;
        }
        
        // 初始化新块
        block = (block_header_t *)new_mem;
        block->size = pages * PAGE_SIZE - sizeof(block_header_t) - sizeof(block_footer_t);
        block->magic = BLOCK_MAGIC;
        block->is_free = true;
        
        // 加入空闲链表
        block->next = free_list;
        block->prev = NULL;
        if (free_list) {
            free_list->prev = block;
        }
        free_list = block;
        
        best_fit = block;
    }
    
    // 分割块（如果需要）
    split_block(best_fit, size);
    
    // 标记为已使用
    best_fit->is_free = false;
    
    mutex_unlock(&mm_lock);
    
    // 返回数据区域指针
    return (void *)((char *)best_fit + sizeof(block_header_t));
}

// 释放内存
void mm_free(void *addr) {
    if (!addr) return;
    
    mutex_lock(&mm_lock);
    
    // 获取块头部
    block_header_t *block = (block_header_t *)((char *)addr - sizeof(block_header_t));
    
    // 验证魔数
    if (block->magic != BLOCK_MAGIC) {
        mutex_unlock(&mm_lock);
        return;
    }
    
    // 标记为空闲
    block->is_free = true;
    
    // 尝试合并相邻的空闲块
    block = coalesce(block);
    
    mutex_unlock(&mm_lock);
} 

// 内存管理系统使用示例

// 1. 动态内存分配
void memory_test(void) {
    // 分配内存
    char *buf1 = mm_alloc(1024);
    int *array = mm_alloc(100 * sizeof(int));
    
    // 使用内存
    strcpy(buf1, "Hello, Memory Management!");
    for (int i = 0; i < 100; i++) {
        array[i] = i;
    }
    
    // 释放内存
    mm_free(buf1);
    mm_free(array);
}

// 2. 内存保护
void protection_test(void) {
    // 分配可读写内存
    void *addr = mm_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS);
    
    // 写入数据
    strcpy(addr, "Protected Memory");
    
    // 修改为只读
    mm_protect(addr, 4096, PROT_READ);
    
    // 尝试写入会触发页面错误
    // strcpy(addr, "This will fail");
    
    // 解除映射
    mm_unmap(addr, 4096);
}

// 3. 页面替换
void page_replacement_test(void) {
    // 初始化页面替换算法
    pra_init(PRA_LRU);
    
    // 分配大量内存，触发页面替换
    void *pages[1000];
    for (int i = 0; i < 1000; i++) {
        pages[i] = pra_alloc_page();
        if (pages[i]) {
            // 访问页面
            memset(pages[i], i, PAGE_SIZE);
            pra_access_page(pages[i]);
        }
    }
    
    // 检查统计信息
    pra_stats_t stats;
    pra_get_stats(&stats);
    printf("Page faults: %d\n", stats.page_faults);
    printf("Page ins: %d\n", stats.page_ins);
    printf("Page outs: %d\n", stats.page_outs);
    printf("Replaced pages: %d\n", stats.replaced_pages);
    
    // 释放页面
    for (int i = 0; i < 1000; i++) {
        if (pages[i]) {
            pra_free_page(pages[i]);
        }
    }
}

// 4. 综合使用场景：简单的内存密集型应用
void memory_intensive_app(void) {
    // 初始化
    mm_init();
    pra_init(PRA_CLOCK);
    
    // 1. 大数组排序
    int *array = mm_alloc(1000000 * sizeof(int));
    for (int i = 0; i < 1000000; i++) {
        array[i] = rand();
    }
    
    // 排序过程会触发大量页面访问
    qsort(array, 1000000, sizeof(int), compare_ints);
    
    // 2. 矩阵运算
    int matrix_size = 1000;
    int **matrix1 = mm_alloc(matrix_size * sizeof(int *));
    int **matrix2 = mm_alloc(matrix_size * sizeof(int *));
    int **result = mm_alloc(matrix_size * sizeof(int *));
    
    for (int i = 0; i < matrix_size; i++) {
        matrix1[i] = mm_alloc(matrix_size * sizeof(int));
        matrix2[i] = mm_alloc(matrix_size * sizeof(int));
        result[i] = mm_alloc(matrix_size * sizeof(int));
        
        // 初始化矩阵
        for (int j = 0; j < matrix_size; j++) {
            matrix1[i][j] = rand();
            matrix2[i][j] = rand();
        }
    }
    
    // 矩阵乘法
    for (int i = 0; i < matrix_size; i++) {
        for (int j = 0; j < matrix_size; j++) {
            result[i][j] = 0;
            for (int k = 0; k < matrix_size; k++) {
                result[i][j] += matrix1[i][k] * matrix2[k][j];
            }
        }
    }
    
    // 3. 内存映射文件操作
    void *file_map = mm_mmap(NULL, 1024*1024, PROT_READ | PROT_WRITE, MAP_PRIVATE);
    
    // 写入数据
    memcpy(file_map, result[0], 1024*1024);
    
    // 修改为只读
    mm_protect(file_map, 1024*1024, PROT_READ);
    
    // 4. 清理资源
    mm_unmap(file_map, 1024*1024);
    
    for (int i = 0; i < matrix_size; i++) {
        mm_free(matrix1[i]);
        mm_free(matrix2[i]);