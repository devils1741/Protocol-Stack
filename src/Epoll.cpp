#include <rte_malloc.h>
#include "Epoll.hpp"
#include "Logger.hpp"
#include "TcpHost.hpp"

int epoll_event_callback(struct event_pool *ep, int sockid, uint32_t event)
{
    struct epitem tmp;
    tmp.sockfd = sockid;
    struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
    if (epi == nullptr)
    {
        SPDLOG_ERROR("Epoll callback error, socket %d not found!", sockid);
        return -1;
    }
    if (epi->rdy)
    {
        epi->event.events |= event;
        return 1;
    }
    SPDLOG_INFO("Epoll socket %d event %u", sockid, event);
    pthread_spin_lock(&ep->slock);
    epi->rdy = 1;
    LIST_INSERT_HEAD(&ep->rdlist, epi, rdlink);
    ep->rdnum++;
    pthread_spin_unlock(&ep->slock);

    pthread_mutex_lock(&ep->cdmtx);
    pthread_cond_signal(&ep->cond);
    pthread_mutex_unlock(&ep->cdmtx);
    return 0;
}

int nepoll_create(int size)
{
    if (size <= 0)
        return -1;
    int fd = EpollNetwork::getinstance().allocFdFromBitMap();
    struct event_pool *ep = (struct event_pool *)rte_malloc("event_pool", sizeof(struct event_pool), 0);
    if (ep == nullptr)
    {
        SPDLOG_ERROR("Epoll create event pool failed!");
        EpollNetwork::getinstance().freeFdFromBitMap(fd);
        return -1;
    }
    TcpTable::getInstance().setEpoll(ep);
    ep->fd = fd;
    ep->rbcnt = 0;
    RB_INIT(&ep->rbr);

    if (pthread_mutex_init(&ep->mtx, nullptr) != 0)
    {
        SPDLOG_ERROR("Epoll create event pool mutex init failed!");
        free(ep);
        EpollNetwork::getinstance().freeFdFromBitMap(fd);
        return -2;
    }
    if (pthread_mutex_init(&ep->cdmtx, NULL))
    {
        pthread_mutex_destroy(&ep->mtx);
        free(ep);
        EpollNetwork::getinstance().freeFdFromBitMap(fd);
        return -2;
    }

    if (pthread_cond_init(&ep->cond, NULL))
    {
        pthread_mutex_destroy(&ep->cdmtx);
        pthread_mutex_destroy(&ep->mtx);
        free(ep);
        EpollNetwork::getinstance().freeFdFromBitMap(fd);
        return -2;
    }

    if (pthread_spin_init(&ep->slock, PTHREAD_PROCESS_SHARED))
    {
        pthread_cond_destroy(&ep->cond);
        pthread_mutex_destroy(&ep->cdmtx);
        pthread_mutex_destroy(&ep->mtx);
        free(ep);
        EpollNetwork::getinstance().freeFdFromBitMap(fd);
        return -2;
    }

    return fd;
}
int nepoll_ctl(int epfd, int op, int sockid, struct epoll_event *event)
{
    struct event_pool *ep = TcpTable::getInstance().getEpoll();
    if (!ep || (!event && op != EPOLL_CTL_DEL))
    {
        SPDLOG_ERROR("Epoll ctl error, epoll not found or event is null!");
        return -1;
    }
    if (op == EPOLL_CTL_ADD)
    {
        pthread_mutex_lock(&ep->mtx);
        struct epitem tmp;
        tmp.sockfd = sockid;
        struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
        if (epi)
        {
            pthread_mutex_unlock(&ep->mtx);
            return -1;
        }
        epi = (struct epitem *)rte_malloc("epitem", sizeof(struct epitem), 0);
        if (!epi)
        {
            pthread_mutex_unlock(&ep->mtx);
            rte_errno = -ENOMEM;
            SPDLOG_ERROR("Epoll ctl add error, malloc epitem failed!");
            return -1;
        }
        epi->sockfd = sockid;
        memcpy(&epi->event, event, sizeof(struct epoll_event));
        ep->rbcnt++;
        pthread_mutex_unlock(&ep->mtx);
    }
    else if (op == EPOLL_CTL_DEL)
    {

        pthread_mutex_lock(&ep->mtx);

        struct epitem tmp;
        tmp.sockfd = sockid;
        struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
        if (!epi)
        {

            pthread_mutex_unlock(&ep->mtx);
            return -1;
        }

        epi = RB_REMOVE(_epoll_rb_socket, &ep->rbr, epi);
        if (!epi)
        {

            pthread_mutex_unlock(&ep->mtx);
            return -1;
        }

        ep->rbcnt--;
        free(epi);

        pthread_mutex_unlock(&ep->mtx);
    }
    else if (op == EPOLL_CTL_MOD)
    {

        struct epitem tmp;
        tmp.sockfd = sockid;
        struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
        if (epi)
        {
            epi->event.events = event->events;
            epi->event.events |= EPOLLERR | EPOLLHUP;
        }
        else
        {
            rte_errno = -ENOENT;
            return -1;
        }
    }
    return 0;
}
int nepoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    struct event_pool *ep = TcpTable::getInstance().getEpoll();
    if (!ep || !events || maxevents <= 0)
    {
        SPDLOG_ERROR("Epoll wait error, epoll not found or events is null or maxevents <= 0!");
        return -1;
    }
    if (pthread_mutex_lock(&ep->cdmtx))
    {
        if (rte_errno == EDEADLK)
            SPDLOG_INFO("Epoll wait error, dead lock!");
    }
    while (ep->rdnum == 0 && timeout != 0)
    {
        ep->waiting = 1;
        if (timeout > 0)
        {
            struct timespec deadline;
            clock_gettime(CLOCK_REALTIME, &deadline);
            if (timeout > 10000)
            {
                int sec = timeout / 1000;
                deadline.tv_sec += sec;
                timeout -= sec * 1000;
            }
            deadline.tv_nsec += timeout * 1000000;

            if (deadline.tv_nsec >= 1000000000)
            {
                deadline.tv_sec += 1;
                deadline.tv_nsec -= 1000000000;
            }
            int ret = pthread_cond_timedwait(&ep->cond, &ep->cdmtx, &deadline);
            if (ret && ret != ETIMEDOUT)
            {
                SPDLOG_ERROR("pthread_cond_timewait");
                pthread_mutex_unlock(&ep->cdmtx);
                return -1;
            }
            timeout = 0;
        }
        else if (timeout < 0)
        {
            int ret = pthread_cond_wait(&ep->cond, &ep->cdmtx);
            if (ret)
            {
                SPDLOG_INFO("pthread_cond_wait");
                pthread_mutex_unlock(&ep->cdmtx);
                return -1;
            }
        }
        ep->waiting = 0;
    }
    pthread_mutex_unlock(&ep->cdmtx);
    pthread_spin_lock(&ep->slock);
    int cnt = 0;
    int num = (ep->rdnum < maxevents) ? ep->rdnum : maxevents;
    int i = 0;
    while (num != 0 && !LIST_EMPTY(&ep->rdlist))
    {
        struct epitem *epi = LIST_FIRST(&ep->rdlist);
        LIST_REMOVE(epi, rdlink);
        epi->rdy = 0;
        memccpy(&events[i++], &epi->event, sizeof(struct epoll_event));
        --num;
        ++cnt;
        --ep->rdnum;
    }
    pthread_spin_lock(&ep->slock);
    return cnt;
}
