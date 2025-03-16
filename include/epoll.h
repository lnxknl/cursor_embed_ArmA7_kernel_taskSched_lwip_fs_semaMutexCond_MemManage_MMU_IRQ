#ifndef EPOLL_H
#define EPOLL_H

#include <stdint.h>
#include <sys/types.h>

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN        0x001
#define EPOLLPRI       0x002
#define EPOLLOUT       0x004
#define EPOLLERR       0x008
#define EPOLLHUP       0x010
#define EPOLLRDNORM    0x040
#define EPOLLRDBAND    0x080
#define EPOLLWRNORM    0x100
#define EPOLLWRBAND    0x200
#define EPOLLMSG       0x400
#define EPOLLRDHUP     0x2000
#define EPOLLEXCLUSIVE 1u << 28
#define EPOLLWAKEUP    1u << 29
#define EPOLLONESHOT   1u << 30
#define EPOLLET        1u << 31

typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t events;
    epoll_data_t data;
};

// 红黑树节点结构
typedef struct rb_node {
    unsigned long rb_parent_color;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
} rb_node_t;

// 红黑树根节点
typedef struct rb_root {
    struct rb_node *rb_node;
} rb_root_t;

// epoll文件描述符信息
typedef struct epitem {
    rb_node_t rbn;           // 红黑树节点
    struct epitem *next;     // 就绪队列链表
    int fd;                  // 文件描述符
    struct epoll_event event;// 事件
} epitem_t;

// epoll实例结构
typedef struct eventpoll {
    rb_root_t rbr;          // 红黑树根
    int maxevents;          // 最大事件数
    int waiting;            // 等待的事件数
    epitem_t *rdlist;       // 就绪列表
    pthread_mutex_t lock;   // 互斥锁
    pthread_cond_t cond;    // 条件变量
} eventpoll_t;

// API函数声明
int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
void epoll_close(int epfd);

#endif 