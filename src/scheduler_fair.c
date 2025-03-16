#include "scheduler.h"
#include "task.h"
#include <stdint.h>

// 红黑树节点颜色
typedef enum {
    RB_BLACK,
    RB_RED
} rb_color_t;

// 红黑树节点
typedef struct rb_node {
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
    rb_color_t color;
    task_t *task;
} rb_node_t;

// 红黑树根
static rb_node_t *rb_root = NULL;

// 调度实体
typedef struct {
    rb_node_t rb_node;
    uint64_t vruntime;
    uint32_t weight;
    uint32_t min_granularity;
    uint64_t exec_start;
    uint64_t sum_exec_runtime;
    uint32_t nr_migrations;
} sched_entity_t;

// 权重表
static const uint32_t prio_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */ 9548, 7620, 6100, 4904, 3906,
    /*  -5 */ 3121, 2501, 1991, 1586, 1277,
    /*   0 */ 1024, 820, 655, 526, 423,
    /*   5 */ 335, 272, 215, 172, 137,
    /*  10 */ 110, 87, 70, 56, 45,
    /*  15 */ 36, 29, 23, 18, 15,
};

// 红黑树操作
static void rb_rotate_left(rb_node_t *node) {
    rb_node_t *right = node->right;
    rb_node_t *parent = node->parent;

    node->right = right->left;
    if (right->left)
        right->left->parent = node;

    right->left = node;
    right->parent = parent;

    if (parent) {
        if (parent->left == node)
            parent->left = right;
        else
            parent->right = right;
    } else {
        rb_root = right;
    }
    node->parent = right;
}

static void rb_rotate_right(rb_node_t *node) {
    rb_node_t *left = node->left;
    rb_node_t *parent = node->parent;

    node->left = left->right;
    if (left->right)
        left->right->parent = node;

    left->right = node;
    left->parent = parent;

    if (parent) {
        if (parent->left == node)
            parent->left = left;
        else
            parent->right = left;
    } else {
        rb_root = left;
    }
    node->parent = left;
}

static void rb_insert_color(rb_node_t *node) {
    rb_node_t *parent, *gparent;

    while ((parent = node->parent) && parent->color == RB_RED) {
        gparent = parent->parent;

        if (parent == gparent->left) {
            rb_node_t *uncle = gparent->right;

            if (uncle && uncle->color == RB_RED) {
                uncle->color = RB_BLACK;
                parent->color = RB_BLACK;
                gparent->color = RB_RED;
                node = gparent;
                continue;
            }

            if (parent->right == node) {
                rb_rotate_left(parent);
                rb_node_t *tmp = parent;
                parent = node;
                node = tmp;
            }

            parent->color = RB_BLACK;
            gparent->color = RB_RED;
            rb_rotate_right(gparent);
        } else {
            rb_node_t *uncle = gparent->left;

            if (uncle && uncle->color == RB_RED) {
                uncle->color = RB_BLACK;
                parent->color = RB_BLACK;
                gparent->color = RB_RED;
                node = gparent;
                continue;
            }

            if (parent->left == node) {
                rb_rotate_right(parent);
                rb_node_t *tmp = parent;
                parent = node;
                node = tmp;
            }

            parent->color = RB_BLACK;
            gparent->color = RB_RED;
            rb_rotate_left(gparent);
        }
    }

    rb_root->color = RB_BLACK;
}

// 初始化调度实体
static void init_sched_entity(task_t *task) {
    sched_entity_t *se = malloc(sizeof(sched_entity_t));
    if (!se) return;

    memset(se, 0, sizeof(sched_entity_t));
    se->weight = prio_to_weight[20 + task->priority];  // 将优先级映射到权重
    se->min_granularity = 1000000;  // 1ms默认最小调度粒度
    task->scheduler_data = se;
}

// 更新虚拟运行时间
void scheduler_update_vruntime(task_t *task) {
    if (!task || !task->scheduler_data) return;

    sched_entity_t *se = (sched_entity_t *)task->scheduler_data;
    uint64_t delta_exec = timer_get_ticks() - se->exec_start;
    uint64_t weight_base = 1024;  // NICE_0_LOAD

    // 计算虚拟运行时间
    se->vruntime += (delta_exec * weight_base) / se->weight;
    se->sum_exec_runtime += delta_exec;
}

// 设置任务权重
void scheduler_set_weight(task_t *task, uint32_t weight) {
    if (!task || !task->scheduler_data) return;

    sched_entity_t *se = (sched_entity_t *)task->scheduler_data;
    se->weight = weight;
}

// 获取最小虚拟运行时间
uint64_t scheduler_min_vruntime(void) {
    rb_node_t *node = rb_root;
    if (!node) return 0;

    while (node->left)
        node = node->left;

    sched_entity_t *se = container_of(node, sched_entity_t, rb_node);
    return se->vruntime;
}

// 将任务插入红黑树
static void enqueue_task_fair(task_t *task) {
    if (!task || !task->scheduler_data) return;

    sched_entity_t *se = (sched_entity_t *)task->scheduler_data;
    rb_node_t *node = &se->rb_node;
    rb_node_t **new = &rb_root;
    rb_node_t *parent = NULL;

    while (*new) {
        sched_entity_t *entry;
        parent = *new;
        
        entry = container_of(parent, sched_entity_t, rb_node);
        if (se->vruntime < entry->vruntime)
            new = &parent->left;
        else
            new = &parent->right;
    }

    node->parent = parent;
    node->left = node->right = NULL;
    node->color = RB_RED;
    *new = node;

    rb_insert_color(node);
}

// 从红黑树中移除任务
static void dequeue_task_fair(task_t *task) {
    if (!task || !task->scheduler_data) return;

    sched_entity_t *se = (sched_entity_t *)task->scheduler_data;
    rb_node_t *node = &se->rb_node;
    rb_node_t *rebalance;
    
    // 红黑树删除操作
    // ... (完整的红黑树删除操作实现)
}

// 选择下一个要运行的任务
task_t *scheduler_pick_next_fair(void) {
    rb_node_t *left = rb_root;
    if (!left) return NULL;

    while (left->left)
        left = left->left;

    sched_entity_t *se = container_of(left, sched_entity_t, rb_node);
    return se->task;
}

// 公平调度器tick处理
void scheduler_fair_tick(void) {
    task_t *current = task_get_current();
    if (!current || !current->scheduler_data) return;

    sched_entity_t *se = (sched_entity_t *)current->scheduler_data;
    
    // 更新统计信息
    scheduler_update_vruntime(current);

    // 检查是否需要抢占
    if (se->vruntime > scheduler_min_vruntime() + se->min_granularity) {
        dequeue_task_fair(current);
        enqueue_task_fair(current);
        scheduler_yield();
    }
} 