#ifndef TCP_HOST_HPP
#define TCP_HOST_HPP

#include <cstdint>
#include <mutex>
#include <rte_ethdev.h>
#include <list>
#include "BaseNetwork.hpp"
#include "Epoll.hpp"

#define TCP_OPTION_LENGTH 10

enum class TCP_STATUS
{
    TCP_STATUS_CLOSED = 0, ///< 初始或者最终状态，连接不存在
    TCP_STATUS_LISTEN,     ///< 服务器端的socket已经绑定端口，等待客户端连接请求
    TCP_STATUS_SYN_SENT,
    TCP_STATUS_SYN_RCVD,
    TCP_STATUS_ESTABLISHED,
    TCP_STATUS_FIN_WAIT_1,
    TCP_STATUS_FIN_WAIT_2,
    TCP_STATUS_CLOSE_WAIT,
    TCP_STATUS_CLOSING,
    TCP_STATUS_LAST_ACK,
    TCP_STATUS_TIME_WAIT,
};

struct TcpStream
{
    int fd;
    uint32_t dstIp;
    uint8_t localMac[RTE_ETHER_ADDR_LEN];
    uint16_t dstPort;
    uint8_t protocol;
    uint16_t srcPort;
    uint32_t srcIp;
    uint32_t sndNxt;
    uint32_t rcvNxt;
    struct rte_ring *sndbuf;
    struct rte_ring *rcvbuf;
    TCP_STATUS status;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
};

struct TcpFragment
{
    uint16_t srcPort;
    uint16_t dstPort;
    uint32_t seqnum;
    uint32_t acknum;
    uint8_t hdrlen_off;
    uint8_t tcp_flags;
    uint16_t windows;
    uint16_t cksum;
    uint16_t tcp_urp;
    int optlen;
    uint32_t option[TCP_OPTION_LENGTH];
    unsigned char *data;
    uint32_t length;
};

class TcpTable
{
public:
    static TcpTable &getInstance()
    {
        static TcpTable instance;
        return instance;
    }
    TcpStream *getTcpStreamByFd(int fd);
    int addTcpStream(TcpStream *ts);
    TcpStream *getTcpStreamByPort(uint16_t port);
    TcpStream *getTcpStream(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport);
    struct event_poll *getEpoll() { return _ep; }
    int removeStream(TcpStream *ts);
    std::list<TcpStream *> getTcpStreamList() { return _tcpStreamList; }
    int getCount() const { return _count; }
    void debug();
    void setEpoll(struct event_poll *ep) { _ep = ep; }

private:
    TcpTable() = default;
    ~TcpTable() = default;
    TcpTable(const TcpTable &) = delete;
    TcpTable &operator=(const TcpTable &) = delete;
    TcpTable(TcpTable &&) = delete;
    TcpTable &operator=(TcpTable &&) = delete;

private:
    int _count;
    std::list<TcpStream *> _tcpStreamList;
    mutable std::mutex _mutex;
    struct event_poll *_ep = nullptr;
};

class TcpServerManager : public BaseNetwork
{
public:
    static TcpServerManager &getInstance()
    {
        static TcpServerManager instance;
        return instance;
    }

    int nsocket(int domain, int type, int protocol);
    int nlisten(int sockfd, __attribute__((unused)) int backlog);
    int nbind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    int naccept(int sockfd, struct sockaddr *addr, __attribute__((unused)) socklen_t *addrlen);
    ssize_t nrecv(int sockfd, void *buf, size_t len, __attribute__((unused)) int flags);
    ssize_t nsend(int sockfd, const void *buf, size_t len, __attribute__((unused)) int flags);
    int nclose(int fd);
    int tcpServer(__attribute__((unused)) void *arg);
};

#endif