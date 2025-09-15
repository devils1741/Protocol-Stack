#ifndef EPOLL_HPP
#define EPOLL_HPP

#include <sys/queue.h>
#include <cstdint>
#include <stddef.h>
#include <thread>
#include "BaseNetwork.hpp"
#include "nty_tree.h"

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

enum EPOLL_EVENTS : uint32_t
{
    EPOLLNONE = 0x0000,
    EPOLLIN = 0x0001,
    EPOLLPRI = 0x0002,
    EPOLLOUT = 0x0004,
    EPOLLERR = 0x0008,
    EPOLLHUP = 0x0010,
    EPOLLRDNORM = 0x0040,
    EPOLLRDBAND = 0x0080,
    EPOLLWRNORM = 0x0100,
    EPOLLWRBAND = 0x0200,
    EPOLLMSG = 0x0400,
    EPOLLREMOVE = 0x1000,
    EPOLLRDHUP = 0x2000,
    EPOLLEXCLUSIVE = (1U << 28),
    EPOLLWAKEUP = (1U << 29),
    EPOLLONESHOT = (1U << 30),
    EPOLLET = (1U << 31)
};

typedef union epoll_data
{
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event
{
    uint32_t events;   /* Epoll events */
    epoll_data_t data; /* User data variable */
};

struct epitem
{
    RB_ENTRY(epitem)
    rbn;
    LIST_ENTRY(epitem)
    rdlink;
    int rdy;
    int sockfd;
    struct epoll_event event;
};

int sockfd_cmp(struct epitem *a, struct epitem *b);

RB_HEAD(_epoll_rb_socket, epitem);
RB_GENERATE_STATIC(_epoll_rb_socket, epitem, rbn, sockfd_cmp);

typedef struct _epoll_rb_socket ep_rb_tree;

struct event_poll
{
    int fd;
    ep_rb_tree rbr;
    int rbcnt;
    LIST_HEAD(, epitem)
    rdlist;
    int rdnum;
    int waiting;
    pthread_mutex_t mtx;
    pthread_spinlock_t slock;
    pthread_cond_t cond;
    pthread_mutex_t cdmtx;
};

int epoll_event_callback(struct event_poll *ep, int sockid, uint32_t event);
int nepoll_create(int size);
int nepoll_ctl(int epfd, int op, int sockfd, struct epoll_event *event);
int nepoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

class EpollNetwork : public BaseNetwork
{
public:
    EpollNetwork() = default;
    ~EpollNetwork() = default;
    static EpollNetwork &getinstance()
    {
        static EpollNetwork instance;
        return instance;
    }
};

#endif