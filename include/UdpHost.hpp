#ifndef UDP_HOST_HPP
#define UDP_HOST_HPP
#include <cstdint>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <list>
#include <mutex>
#include "BaseNetwork.hpp"

struct UdpHost
{
    int fd;                               ///< 文件描述符
    uint32_t localIp;                     ///< 本地IP地址
    uint8_t localMac[RTE_ETHER_ADDR_LEN]; ///< 本地MAC地址
    uint16_t localport;                   ///< 本地端口
    uint8_t protocal;                     ///< 协议类型
    struct rte_ring *sndbuf;              ///< 发送缓冲区
    struct rte_ring *rcvbuf;              ///< 接收缓冲区
    pthread_cond_t cond;                  ///< 条件变量，用于线程间同步
    pthread_mutex_t mutex;                ///< 互斥锁，用于保护条件变量
};

struct offload
{
    uint32_t sip;
    uint32_t dip;
    uint16_t sport;
    uint16_t dport;
    int protocol;
    unsigned char *data;
    uint16_t length;
};

class UdpServerManager : public BaseNetwork
{
public:
    static UdpServerManager &getInstance()
    {
        static UdpServerManager instance;
        return instance;
    }
    struct UdpHost *getHostInfoFromIpAndPort(uint32_t dip, uint16_t port, uint8_t proto);

    int nsocket(__attribute__((unused)) int domain, int type, __attribute__((unused)) int protocol);
    int nbind(int sockfd, const struct sockaddr *addr, __attribute__((unused)) socklen_t addrlen);
    ssize_t nrecvfrom(int sockfd, void *buf, size_t len, __attribute__((unused)) int flags, struct sockaddr *src_addr, __attribute__((unused)) socklen_t *addrlen);
    ssize_t nsendto(int sockfd, const void *buf, size_t len, __attribute__((unused)) int flags, const struct sockaddr *dest_addr, __attribute__((unused)) socklen_t addrlen);
    int nclose(int fd);
    int udpServer(__attribute__((unused)) void *arg);
    std::list<UdpHost *> &getUdpHostList();
    UdpHost *getHostInfoFromFd(int fd);
    int removeHost(UdpHost *host);

private:
    UdpServerManager() = default;
    ~UdpServerManager() = default;
    UdpServerManager(const UdpServerManager &) = delete;
    UdpServerManager &operator=(const UdpServerManager &) = delete;
    UdpServerManager(UdpServerManager &&) = delete;
    UdpServerManager &operator=(UdpServerManager &&) = delete;

private:
    std::list<UdpHost *> _udpHostList;
    std::mutex _mutex;
};

#endif