#ifndef __MM_H__
#define __MM_H__

#include <stdint.h>
#include <stdbool.h>

// 内存页大小
#define PAGE_SIZE           4096
#define PAGE_SHIFT         12
#define PAGE_MASK          (~(PAGE_SIZE - 1))

// 内存保护标志
#define PROT_NONE          0x0     // 页面不可访问
#define PROT_READ          0x1     // 页面可读
#define PROT_WRITE         0x2     // 页面可写
#define PROT_EXEC          0x4     // 页面可执行

// 内存映射标志
#define MAP_PRIVATE        0x01    // 私有映射
#define MAP_SHARED         0x02    // 共享映射
#define MAP_FIXED          0x04    // 固定地址映射
#define MAP_ANONYMOUS      0x08    // 匿名映射

// 页面状态
typedef enum {
    PAGE_FREE = 0,         // 空闲页面
    PAGE_ALLOCATED,        // 已分配页面
    PAGE_SWAPPED,         // 已换出页面
    PAGE_RESERVED         // 保留页面
} page_state_t;

// 页面标志
typedef struct {
    uint32_t present : 1;  // 页面是否在内存中
    uint32_t writable : 1; // 页面是否可写
    uint32_t user : 1;     // 用户态是否可访问
    uint32_t accessed : 1; // 页面是否被访问
    uint32_t dirty : 1;    // 页面是否被修改
    uint32_t reserved : 7; // 保留位
    uint32_t frame : 20;   // 物理页帧号
} page_flags_t;

// 页表项
typedef struct {
    page_flags_t flags;
    uint32_t swap_offset;  // 换出页面在交换区的偏移
} pte_t;

// 页目录项
typedef struct {
    page_flags_t flags;
    pte_t *page_table;     // 指向页表
} pde_t;

// 虚拟内存区域
typedef struct vm_area {
    uint32_t start;        // 起始地址
    uint32_t end;          // 结束地址
    uint32_t flags;        // 访问权限
    struct vm_area *next;  // 下一个区域
} vm_area_t;

// 内存描述符
typedef struct {
    pde_t *pgd;           // 页目录
    vm_area_t *mmap;      // 虚拟内存区域链表
    uint32_t start_code;  // 代码段起始
    uint32_t end_code;    // 代码段结束
    uint32_t start_data;  // 数据段起始
    uint32_t end_data;    // 数据段结束
    uint32_t start_brk;   // 堆起始
    uint32_t brk;         // 堆当前结束
    uint32_t start_stack; // 栈起始
} mm_struct_t;

// 页面替换算法类型
typedef enum {
    PRA_FIFO = 0,        // 先进先出
    PRA_CLOCK,           // 时钟算法
    PRA_LRU,            // 最近最少使用
    PRA_NFU             // 最不常用
} pra_type_t;

// 页面替换统计信息
typedef struct {
    uint32_t page_faults;     // 缺页次数
    uint32_t page_ins;        // 页面调入次数
    uint32_t page_outs;       // 页面换出次数
    uint32_t replaced_pages;  // 被替换页面数
} pra_stats_t;

// 内存管理函数
void mm_init(void);
void *mm_alloc_pages(uint32_t count);
void mm_free_pages(void *addr, uint32_t count);
void *mm_alloc(size_t size);
void mm_free(void *addr);

// 虚拟内存管理函数
int mm_map(void *addr, size_t length, int prot, int flags);
int mm_unmap(void *addr, size_t length);
int mm_protect(void *addr, size_t length, int prot);
void *mm_mmap(void *addr, size_t length, int prot, int flags);

// 页面替换函数
void pra_init(pra_type_t type);
void *pra_alloc_page(void);
void pra_free_page(void *addr);
void pra_access_page(void *addr);
void pra_get_stats(pra_stats_t *stats);

// 调试函数
void mm_dump_stats(void);
void mm_check_leaks(void);
void mm_debug_info(void);

#endif 