#include "BaseNetwork.hpp"
#include <arpa/inet.h>
#include <rte_malloc.h>
#include "ConfigManager.hpp"

#define DEFAULT_FD_NUM 3
#define MAX_FD_COUNT 1024
unsigned char BaseNetwork::_fdTable[1024] = {0};
std::list<LocalHost> _hostList;

int BaseNetwork::getFdFromBitMap()
{
    int fd = DEFAULT_FD_NUM;
    for (; fd < MAX_FD_COUNT; ++fd)
    {
        if ((_fdTable[int(fd / 8)] & (0x1 << fd % 8)) == 0)
        {
            _fdTable[(int)(fd / 8)] |= (0x1 << fd % 8);
            return fd;
        }
    }
    return -1;
}

int BaseNetwork::freeFdFromBitMap(int &fd)
{
    if (fd < DEFAULT_FD_NUM || fd >= MAX_FD_COUNT)
        return -1;
    if ((_fdTable[int(fd / 8)] & (0x1 << fd % 8)) == 0)
        return -1;
    _fdTable[int(fd / 8)] &= ~(0x1 << fd % 8);
    return 0;
}

int BaseNetwork::createSocket(__attribute__((unused)) int domain, int type, __attribute__((unused)) int protocol)
{
    int fd = getFdFromBitMap();
    const int RING_SIZE = ConfigManager::getInstance().getRingSize();

    if (type == SOCK_DGRAM)
    {
        struct LocalHost *localHost = static_cast<LocalHost *>(rte_malloc("localHost", sizeof(struct LocalHost), 0));
        if (localHost == nullptr)
        {
            return -1;
        }
        memset(localHost, 0, sizeof(struct LocalHost));
        localHost->fd = fd;
        localHost->protocal = IPPROTO_UDP;
        localHost->rcvbuf = rte_ring_create("recv buffer", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (localHost->rcvbuf == NULL)
        {
            rte_free(localHost);
            return -1;
        }

        localHost->sndbuf = rte_ring_create("send buffer", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (localHost->sndbuf == NULL)
        {
            rte_ring_free(localHost->rcvbuf);
            rte_free(localHost);
            return -1;
        }
        pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
        rte_memcpy(&localHost->cond, &blank_cond, sizeof(pthread_cond_t));

        pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
        rte_memcpy(&localHost->mutex, &blank_mutex, sizeof(pthread_mutex_t));

        _hostList.emplace_back(localHost);
    }
    return fd;
}