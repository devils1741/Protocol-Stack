#include "UdpHost.hpp"
#include <rte_malloc.h>
#include "ConfigManager.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#define UDP_APP_RECV_BUFFER_SIZE 128

struct UdpHost *UdpServerManager::getHostInfoFromIpAndPort(uint32_t dip, uint16_t port, uint8_t proto)
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto &host : _udpHostList)
    {
        if (host->localIp == dip && host->localport == port && host->protocal == proto)
        {
            return host;
        }
    }
    return nullptr;
}

int UdpServerManager::nsocket(__attribute__((unused)) int domain, int type, __attribute__((unused)) int protocol)
{
    SPDLOG_INFO("create socket,domain:{}, type: {}, protocol {}", domain, type, protocol);
    int fd = allocFdFromBitMap();
    const int RING_SIZE = ConfigManager::getInstance().getRingSize();

    if (type == SOCK_DGRAM)
    {
        struct UdpHost *udpHost = static_cast<UdpHost *>(rte_malloc("UdpHost", sizeof(struct UdpHost), 0));
        if (udpHost == nullptr)
        {
            return -1;
        }
        memset(udpHost, 0, sizeof(struct UdpHost));
        udpHost->fd = fd;
        udpHost->protocal = IPPROTO_UDP;
        udpHost->rcvbuf = rte_ring_create("recv buffer", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (udpHost->rcvbuf == NULL)
        {
            rte_free(udpHost);
            return -1;
        }

        udpHost->sndbuf = rte_ring_create("send buffer", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (udpHost->sndbuf == NULL)
        {
            rte_ring_free(udpHost->rcvbuf);
            rte_free(udpHost);
            return -1;
        }
        pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
        rte_memcpy(&udpHost->cond, &blank_cond, sizeof(pthread_cond_t));

        pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
        rte_memcpy(&udpHost->mutex, &blank_mutex, sizeof(pthread_mutex_t));

        _udpHostList.emplace_back(udpHost);
    }
    else
    {
        return -1;
    }
    return fd;
}

int UdpServerManager::nbind(int sockfd, const struct sockaddr *addr, __attribute__((unused)) socklen_t addrlen)
{
    SPDLOG_INFO("Bind socket fd: {}, addr: {}", sockfd, sockaddr_in_to_string(*(const struct sockaddr_in *)addr));
    int isExist = searchFdFromBitMap(sockfd);
    if (isExist == 0)
    {
        return -1; // 文件描述符不存在
    }
    for (auto &it : _udpHostList)
    {
        if (it->fd == sockfd)
        {
            struct UdpHost *host = (struct UdpHost *)it;
            const struct sockaddr_in *laddr = (const struct sockaddr_in *)addr;
            host->localport = laddr->sin_port;
            rte_memcpy(&host->localIp, &laddr->sin_addr.s_addr, sizeof(uint32_t));
            rte_memcpy(host->localMac, ConfigManager::getInstance().getSrcMac(), RTE_ETHER_ADDR_LEN);
            return 0; // 成功绑定
        }
    }
    return -1;
}

int UdpServerManager::nclose(int fd)
{
    SPDLOG_INFO("Close fd: {}", fd);
    void *hostinfo = getHostInfoFromFd(fd);
    if (hostinfo == NULL)
        return -1;
    struct UdpHost *host = (struct UdpHost *)hostinfo;
    _udpHostList.remove(host);
    if (host->rcvbuf)
    {
        rte_ring_free(host->rcvbuf);
    }
    if (host->sndbuf)
    {
        rte_ring_free(host->sndbuf);
    }
    rte_free(host);
    freeFdFromBitMap(fd);
    return 0;
}

int UdpServerManager::udpServer(__attribute__((unused)) void *arg)
{
    int connfd = nsocket(AF_INET, SOCK_DGRAM, 0);
    if (connfd == -1)
    {
        SPDLOG_ERROR("Create sockfd failed\n");
        return -1;
    }
    SPDLOG_INFO("Create sockfd success, fd: {}", connfd);

    struct sockaddr_in localaddr, clientaddr; // struct sockaddr
    memset(&localaddr, 0, sizeof(struct sockaddr_in));
    localaddr.sin_port = htons(8888);
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = inet_addr("192.168.0.104");
    nbind(connfd, (struct sockaddr *)&localaddr, sizeof(localaddr));

    SPDLOG_INFO("local host: {}", sockaddr_in_to_string(localaddr));

    char buffer[UDP_APP_RECV_BUFFER_SIZE] = {0};
    socklen_t addrlen = sizeof(clientaddr);
    while (1)
    {
        if (nrecvfrom(connfd, buffer, UDP_APP_RECV_BUFFER_SIZE, 0,
                      (struct sockaddr *)&clientaddr, &addrlen) < 0)
        {
            continue;
        }
        else
        {
            SPDLOG_INFO("Received packet from {}:{} data:{}", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), buffer);
            nsendto(connfd, buffer, strlen(buffer), 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr));
        }
    }

    nclose(connfd);
    return 0;
}

ssize_t UdpServerManager::nrecvfrom(int sockfd, void *buf, size_t len, __attribute__((unused)) int flags,
                                    struct sockaddr *src_addr, __attribute__((unused)) socklen_t *addrlen)
{

    struct UdpHost *host = getHostInfoFromFd(sockfd);
    if (host == NULL)
        return -1;

    struct offload *ol = nullptr;
    unsigned char *ptr = nullptr;

    struct sockaddr_in *saddr = (struct sockaddr_in *)src_addr;

    int nb = -1;
    pthread_mutex_lock(&host->mutex);
    while ((nb = rte_ring_mc_dequeue(host->rcvbuf, (void **)&ol)) < 0)
    {
        pthread_cond_wait(&host->cond, &host->mutex);
    }
    pthread_mutex_unlock(&host->mutex);

    saddr->sin_port = ol->sport;
    rte_memcpy(&saddr->sin_addr.s_addr, &ol->sip, sizeof(uint32_t));

    if (len < ol->length)
    {
        rte_memcpy(buf, ol->data, len);
        ptr = (unsigned char *)rte_malloc("unsigned char *", ol->length - len, 0);
        rte_memcpy(ptr, ol->data + len, ol->length - len);
        ol->length -= len;
        rte_free(ol->data);
        ol->data = ptr;
        rte_ring_mp_enqueue(host->rcvbuf, ol);

        return len;
    }
    else
    {
        rte_memcpy(buf, ol->data, ol->length);
        rte_free(ol->data);
        rte_free(ol);
        return ol->length;
    }
}

ssize_t UdpServerManager::nsendto(int sockfd, const void *buf, size_t len, __attribute__((unused)) int flags,
                                  const struct sockaddr *dest_addr, __attribute__((unused)) socklen_t addrlen)
{

    struct UdpHost *host = getHostInfoFromFd(sockfd);
    if (host == NULL)
        return -1;

    const struct sockaddr_in *daddr = (const struct sockaddr_in *)dest_addr;

    struct offload *ol = (struct offload *)rte_malloc("offload", sizeof(struct offload), 0);
    if (ol == NULL)
        return -1;

    ol->dip = daddr->sin_addr.s_addr;
    ol->dport = daddr->sin_port;
    ol->sip = host->localIp;
    ol->sport = host->localport;
    ol->length = len;

    struct in_addr addr;
    addr.s_addr = ol->dip;
    SPDLOG_INFO("Send packet to {}:{}", inet_ntoa(addr), ntohs(ol->dport));

    ol->data = (unsigned char *)rte_malloc("unsigned char *", len, 0);
    if (ol->data == NULL)
    {
        rte_free(ol);
        return -1;
    }
    rte_memcpy(ol->data, buf, len);
    rte_ring_mp_enqueue(host->sndbuf, ol);

    return len;
}

std::list<UdpHost *> &UdpServerManager::getUdpHostList()
{
    return _udpHostList;
}

UdpHost *UdpServerManager::getHostInfoFromFd(int fd)
{
    for (UdpHost *it : _udpHostList)
    {
        if (it->fd == fd)
            return it;
    }
    return nullptr;
}
