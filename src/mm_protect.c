#include "mm.h"
#include "mmu.h"
#include "interrupt.h"
#include <string.h>

// 页面错误处理函数
void page_fault_handler(uint32_t fault_addr, uint32_t error_code) {
    mm_struct_t *mm = task_get_current()->mm;
    
    // 查找对应的虚拟内存区域
    vm_area_t *vma = mm->mmap;
    while (vma) {
        if (fault_addr >= vma->start && fault_addr < vma->end) {
            break;
        }
        vma = vma->next;
    }
    
    if (!vma) {
        // 访问非法地址
        task_exit(-1);
        return;
    }
    
    // 检查访问权限
    if ((error_code & 2) && !(vma->flags & PROT_WRITE)) {
        // 写保护错误
        task_exit(-1);
        return;
    }
    
    if (!(error_code & 1) && !(vma->flags & PROT_READ)) {
        // 读保护错误
        task_exit(-1);
        return;
    }
    
    if ((error_code & 4) && !(vma->flags & PROT_EXEC)) {
        // 执行保护错误
        task_exit(-1);
        return;
    }
    
    // 分配新页面
    void *page = pra_alloc_page();
    if (!page) {
        task_exit(-1);
        return;
    }
    
    // 建立映射
    uint32_t prot = 0;
    if (vma->flags & PROT_READ) prot |= MMU_PERM_READ;
    if (vma->flags & PROT_WRITE) prot |= MMU_PERM_WRITE;
    if (vma->flags & PROT_EXEC) prot |= MMU_PERM_EXEC;
    
    mmu_map_page(fault_addr & PAGE_MASK, (uint32_t)page, prot);
}

// 设置内存保护
int mm_protect(void *addr, size_t length, int prot) {
    uint32_t start = (uint32_t)addr & PAGE_MASK;
    uint32_t end = ((uint32_t)addr + length + PAGE_SIZE - 1) & PAGE_MASK;
    
    mm_struct_t *mm = task_get_current()->mm;
    
    // 查找并更新虚拟内存区域
    vm_area_t *vma = mm->mmap;
    while (vma) {
        if (start >= vma->start && end <= vma->end) {
            // 更新保护标志
            vma->flags = prot;
            
            // 更新页表项
            for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
                pte_t *pte = mmu_get_pte(addr);
                if (pte && pte->flags.present) {
                    uint32_t new_prot = 0;
                    if (prot & PROT_READ) new_prot |= MMU_PERM_READ;
                    if (prot & PROT_WRITE) new_prot |= MMU_PERM_WRITE;
                    if (prot & PROT_EXEC) new_prot |= MMU_PERM_EXEC;
                    
                    mmu_update_prot(addr, new_prot);
                }
            }
            
            return 0;
        }
        vma = vma->next;
    }
    
    return -1;
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