#include "mini_malloc.h"
#include "mini_syscall.h"

#define ALIGN(x) (((x) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define BLOCK_SIZE sizeof(block_t)

typedef struct block {
    size_t size;
    struct block *next;
    struct block *prev;
    int free;
    char data[1];
} block_t;

static block_t *heap_start = NULL;
static block_t *heap_end = NULL;

static block_t *find_free_block(size_t size)
{
    block_t *block = heap_start;
    while (block) {
        if (block->free && block->size >= size)
            return block;
        block = block->next;
    }
    return NULL;
}

static void split_block(block_t *block, size_t size)
{
    block_t *new_block;
    if (block->size - size >= BLOCK_SIZE + sizeof(size_t)) {
        new_block = (block_t *)((char *)block + BLOCK_SIZE + size);
        new_block->size = block->size - size - BLOCK_SIZE;
        new_block->next = block->next;
        new_block->prev = block;
        new_block->free = 1;
        block->size = size;
        block->next = new_block;
        if (new_block->next)
            new_block->next->prev = new_block;
        else
            heap_end = new_block;
    }
}

static block_t *extend_heap(size_t size)
{
    block_t *block;
    size_t total_size = ALIGN(size + BLOCK_SIZE);
    
    block = (block_t *)sys_brk(0);
    if (sys_brk((char *)block + total_size) == (void *)-1)
        return NULL;
    
    block->size = size;
    block->next = NULL;
    block->prev = heap_end;
    block->free = 0;
    
    if (heap_end)
        heap_end->next = block;
    else
        heap_start = block;
    
    heap_end = block;
    return block;
}

static void merge_blocks(block_t *block)
{
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next)
            block->next->prev = block;
        else
            heap_end = block;
    }
    if (block->prev && block->prev->free) {
        block->prev->size += BLOCK_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next)
            block->next->prev = block->prev;
        else
            heap_end = block->prev;
        block = block->prev;
    }
}

void *malloc(size_t size)
{
    block_t *block;
    
    if (size == 0)
        return NULL;
    
    size = ALIGN(size);
    
    if ((block = find_free_block(size))) {
        block->free = 0;
        split_block(block, size);
    } else {
        block = extend_heap(size);
        if (!block)
            return NULL;
    }
    
    return block->data;
}

void free(void *ptr)
{
    block_t *block;
    
    if (!ptr)
        return;
    
    block = (block_t *)((char *)ptr - BLOCK_SIZE);
    block->free = 1;
    merge_blocks(block);
}

void *realloc(void *ptr, size_t size)
{
    block_t *block;
    void *new_ptr;
    
    if (!ptr)
        return malloc(size);
    
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    block = (block_t *)((char *)ptr - BLOCK_SIZE);
    if (block->size >= size) {
        split_block(block, size);
        return ptr;
    }
    
    new_ptr = malloc(size);
    if (!new_ptr)
        return NULL;
    
    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total_size = nmemb * size;
    void *ptr = malloc(total_size);
    if (ptr)
        memset(ptr, 0, total_size);
    return ptr;
} 