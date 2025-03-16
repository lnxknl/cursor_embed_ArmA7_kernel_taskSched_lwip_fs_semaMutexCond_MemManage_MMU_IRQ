#include "epoll.h"
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#define MAX_EPOLL_INSTANCES 1024
static eventpoll_t *epoll_instances[MAX_EPOLL_INSTANCES];
static pthread_mutex_t instance_lock = PTHREAD_MUTEX_INITIALIZER;

static int find_available_fd(void)
{
    pthread_mutex_lock(&instance_lock);
    for (int i = 0; i < MAX_EPOLL_INSTANCES; i++) {
        if (!epoll_instances[i]) {
            pthread_mutex_unlock(&instance_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&instance_lock);
    return -1;
}

int epoll_create(int size)
{
    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }

    int epfd = find_available_fd();
    if (epfd < 0) {
        errno = EMFILE;
        return -1;
    }

    eventpoll_t *ep = calloc(1, sizeof(eventpoll_t));
    if (!ep) {
        errno = ENOMEM;
        return -1;
    }

    ep->maxevents = size;
    ep->rbr.rb_node = NULL;
    ep->rdlist = NULL;
    pthread_mutex_init(&ep->lock, NULL);
    pthread_cond_init(&ep->cond, NULL);

    pthread_mutex_lock(&instance_lock);
    epoll_instances[epfd] = ep;
    pthread_mutex_unlock(&instance_lock);

    return epfd;
}

static epitem_t *find_epitem(eventpoll_t *ep, int fd)
{
    struct rb_node *node = ep->rbr.rb_node;
    
    while (node) {
        epitem_t *epi = container_of(node, epitem_t, rbn);
        
        if (fd < epi->fd)
            node = node->rb_left;
        else if (fd > epi->fd)
            node = node->rb_right;
        else
            return epi;
    }
    return NULL;
}

static void insert_epitem(eventpoll_t *ep, epitem_t *epi)
{
    struct rb_node **p = &ep->rbr.rb_node;
    struct rb_node *parent = NULL;
    epitem_t *epic;

    while (*p) {
        parent = *p;
        epic = container_of(parent, epitem_t, rbn);
        
        if (epi->fd < epic->fd)
            p = &(*p)->rb_left;
        else
            p = &(*p)->rb_right;
    }

    rb_link_node(&epi->rbn, parent, p);
    rb_insert_color(&epi->rbn, &ep->rbr);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
    if (epfd < 0 || epfd >= MAX_EPOLL_INSTANCES || !epoll_instances[epfd]) {
        errno = EBADF;
        return -1;
    }

    eventpoll_t *ep = epoll_instances[epfd];
    pthread_mutex_lock(&ep->lock);

    epitem_t *epi = find_epitem(ep, fd);
    int error = 0;

    switch (op) {
        case EPOLL_CTL_ADD:
            if (epi) {
                error = EEXIST;
                break;
            }
            
            epi = calloc(1, sizeof(epitem_t));
            if (!epi) {
                error = ENOMEM;
                break;
            }
            
            epi->fd = fd;
            epi->event = *event;
            insert_epitem(ep, epi);
            break;

        case EPOLL_CTL_MOD:
            if (!epi) {
                error = ENOENT;
                break;
            }
            epi->event = *event;
            break;

        case EPOLL_CTL_DEL:
            if (!epi) {
                error = ENOENT;
                break;
            }
            rb_erase(&epi->rbn, &ep->rbr);
            free(epi);
            break;

        default:
            error = EINVAL;
    }

    pthread_mutex_unlock(&ep->lock);
    
    if (error) {
        errno = error;
        return -1;
    }
    return 0;
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    if (epfd < 0 || epfd >= MAX_EPOLL_INSTANCES || !epoll_instances[epfd] ||
        !events || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }

    eventpoll_t *ep = epoll_instances[epfd];
    struct timespec ts;
    int ready = 0;

    pthread_mutex_lock(&ep->lock);

    if (timeout > 0) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (timeout % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
    }

    // 检查就绪列表
    epitem_t *epi = ep->rdlist;
    while (epi && ready < maxevents) {
        events[ready++] = epi->event;
        epi = epi->next;
    }

    // 如果没有就绪事件且需要等待
    if (!ready && timeout != 0) {
        ep->waiting = 1;
        if (timeout > 0)
            pthread_cond_timedwait(&ep->cond, &ep->lock, &ts);
        else if (timeout < 0)
            pthread_cond_wait(&ep->cond, &ep->lock);
        ep->waiting = 0;

        // 重新检查就绪列表
        epi = ep->rdlist;
        while (epi && ready < maxevents) {
            events[ready++] = epi->event;
            epi = epi->next;
        }
    }

    pthread_mutex_unlock(&ep->lock);
    return ready;
}

void epoll_close(int epfd)
{
    if (epfd < 0 || epfd >= MAX_EPOLL_INSTANCES)
        return;

    pthread_mutex_lock(&instance_lock);
    eventpoll_t *ep = epoll_instances[epfd];
    if (ep) {
        epoll_instances[epfd] = NULL;
        
        // 清理所有epitem
        struct rb_node *node;
        while ((node = ep->rbr.rb_node)) {
            epitem_t *epi = container_of(node, epitem_t, rbn);
            rb_erase(node, &ep->rbr);
            free(epi);
        }

        pthread_mutex_destroy(&ep->lock);
        pthread_cond_destroy(&ep->cond);
        free(ep);
    }
    pthread_mutex_unlock(&instance_lock);
} 