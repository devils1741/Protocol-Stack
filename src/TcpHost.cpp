#include "TcpHost.hpp"
#include "ConfigManager.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <rte_malloc.h>
#include <arpa/inet.h>
#include <vector>

#define TCP_OPTION_LENGTH 10
#define TCP_MAX_SEQ 4294967295
#define TCP_INITIAL_WINDOW 14600

int TcpServerManager::tcpServer(__attribute__((unused)) void *arg)
{
    SPDLOG_INFO("TCP server thread start successed");
    int listenfd = nsocket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        SPDLOG_ERROR("tcp socket create failed");
        return -1;
    }
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(struct sockaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(9999);
    nbind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    SPDLOG_INFO("TCP server bind to port: {}", ntohs(servaddr.sin_port));
    nlisten(listenfd, 10);
    int BUFFER_SIZE = ConfigManager::getInstance().getBufferSize();
    while (1)
    {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        std::vector<char> buff(BUFFER_SIZE);
        int connfd = naccept(listenfd, (struct sockaddr *)&client, &len);
        if (connfd < 0)
        {
            SPDLOG_ERROR("TCP accept failed: {}", strerror(errno));
            continue;
        }
        SPDLOG_INFO("connfd: {}", connfd);
        while (1)
        {
            int n = nrecv(connfd, buff.data(), BUFFER_SIZE, 0); // block
            if (n > 0)
            {
                SPDLOG_INFO("TCP recv data: {}", buff.data());
                nsend(connfd, buff.data(), n, 0);
            }
            else if (n == 0)
            {
                nclose(connfd);
                break;
            }
            else
            {
            }
        }
    }
    nclose(listenfd);
}

int TcpServerManager::nsocket(__attribute__((unused)) int domain, int type, __attribute__((unused)) int protocol)
{
    SPDLOG_INFO("create tcp socket,domain:{}, type: {}, protocol {}", domain, type, protocol);
    int fd = allocFdFromBitMap();
    if (fd == -1)
    {
        SPDLOG_INFO("socket fd alloc failed");
        return -1;
    }
    const int RING_SIZE = ConfigManager::getInstance().getRingSize();

    if (type == SOCK_STREAM)
    {
        struct TcpStream *ts = static_cast<TcpStream *>(rte_malloc("TcpStream", sizeof(struct TcpStream), 0));
        if (ts == nullptr)
        {
            return -1;
        }
        memset(ts, 0, sizeof(struct TcpStream));
        ts->fd = fd;
        ts->protocol = IPPROTO_TCP;
        ts->rcvbuf = rte_ring_create("tcp recv buffer", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (ts->rcvbuf == nullptr)
        {
            rte_free(ts);
            return -1;
        }

        ts->sndbuf = rte_ring_create("tcp send buffer", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (ts->sndbuf == nullptr)
        {
            rte_ring_free(ts->rcvbuf);
            rte_free(ts);
            return -1;
        }
        pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
        rte_memcpy(&ts->cond, &blank_cond, sizeof(pthread_cond_t));

        pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
        rte_memcpy(&ts->mutex, &blank_mutex, sizeof(pthread_mutex_t));
        TcpTable::getInstance().addTcpStream(ts);
        SPDLOG_INFO("TCP Stream created with fd: {}", ts->fd);
    }
    else
    {
        SPDLOG_ERROR("Unsupported socket type: {}", type);
        return -1;
    }
    return fd;
}

int TcpServerManager::nlisten(int sockfd, __attribute__((unused)) int backlog)
{
    TcpStream *ts = TcpTable::getInstance().getTcpStreamByFd(sockfd);
    if (ts == nullptr)
    {
        SPDLOG_ERROR("Couldn't found TCP Stream. sockfd:{}", sockfd);
        return -1;
    }
    ts->status = TCP_STATUS::TCP_STATUS_LISTEN;
    return 0;
}

int TcpServerManager::nbind(int sockfd, const struct sockaddr *addr, __attribute__((unused)) socklen_t addrlen)
{
    SPDLOG_INFO("Bind socket fd: {}, addr: {}", sockfd, sockaddr_in_to_string(*(const struct sockaddr_in *)addr));
    TcpStream *ts = TcpTable::getInstance().getTcpStreamByFd(sockfd);
    if (ts == nullptr)
    {
        SPDLOG_ERROR("Couldn't found TCP Stream. sockfd:{}", sockfd);
        return -1;
    }
    const struct sockaddr_in *laddr = (const struct sockaddr_in *)addr;
    ts->dstPort = laddr->sin_port;
    rte_memcpy(&ts->dstIp, &laddr->sin_addr.s_addr, sizeof(uint32_t));
    rte_memcpy(ts->localMac, ConfigManager::getInstance().getSrcMac(), RTE_ETHER_ADDR_LEN);
    ts->status = TCP_STATUS::TCP_STATUS_CLOSED;
    return 0;
}

ssize_t TcpServerManager::nrecv(int sockfd, void *buf, size_t len, __attribute__((unused)) int flags)
{
    SPDLOG_INFO("nrecv sockfd: {}", sockfd);
    ssize_t length = 0;
    TcpStream *ts = TcpTable::getInstance().getTcpStreamByFd(sockfd);
    if (ts == nullptr)
    {
        SPDLOG_ERROR("Couldn't found TCP Stream. sockfd:{}", sockfd);
        return -1;
    }
    struct TcpFragment *tf = nullptr;
    int nb_rcv = 0;
    TcpTable::getInstance().debug();
    pthread_mutex_lock(&ts->mutex);
    while ((nb_rcv = rte_ring_mc_dequeue(ts->rcvbuf, (void **)&tf)) < 0)
    {
        SPDLOG_INFO("Waiting for data...");
        pthread_cond_wait(&ts->cond, &ts->mutex);
    }
    pthread_mutex_unlock(&ts->mutex);

    if (tf->length > len)
    {
        SPDLOG_INFO("Received TCP fragment length is larger than buffer length, len: {}, tf->length: {}", len, tf->length);
        rte_memcpy(buf, tf->data, len);
        uint32_t i = 0;
        for (i = 0; i < tf->length - len; i++)
        {
            tf->data[i] = tf->data[len + i];
        }
        tf->length = tf->length - len;
        length = tf->length;
        rte_ring_mp_enqueue(ts->rcvbuf, tf);
    }
    else if (tf->length == 0)
    {
        SPDLOG_INFO("Received empty TCP fragment, free it");
        rte_free(tf);
        return 0;
    }
    else
    {
        SPDLOG_INFO("Received TCP fragment length: {}, copy to buffer", tf->length);
        rte_memcpy(buf, tf->data, tf->length);
        length = tf->length;
        rte_free(tf->data);
        tf->data = nullptr;
        rte_free(tf);
    }
    return length;
}

int TcpServerManager::naccept(int sockfd, struct sockaddr *addr, __attribute__((unused)) socklen_t *addrlen)
{
    SPDLOG_INFO("naccept sockfd: {}", sockfd);
    TcpStream *ts = TcpTable::getInstance().getTcpStreamByFd(sockfd);
    if (ts == nullptr)
    {
        SPDLOG_ERROR("Couldn't found TCP Stream. sockfd:{}", sockfd);
        return -1;
    }
    if (ts->protocol == IPPROTO_TCP)
    {
        TcpStream *tmp = nullptr;
        pthread_mutex_lock(&ts->mutex);
        SPDLOG_INFO("Waiting for TCP connection on port: {}", ntohs(ts->dstPort));
        while ((tmp = TcpTable::getInstance().getTcpStreamByPort(ts->dstPort)) == nullptr)
        {
            pthread_cond_wait(&ts->cond, &ts->mutex);
        }
        pthread_mutex_unlock(&ts->mutex);
        tmp->fd = allocFdFromBitMap();
        SPDLOG_DEBUG("alloc fd {}", tmp->fd);
        struct sockaddr_in *saddr = (struct sockaddr_in *)addr;
        saddr->sin_port = tmp->srcPort;
        rte_memcpy(&saddr->sin_addr.s_addr, &tmp->srcIp, sizeof(uint32_t));
        TcpTable::getInstance().debug();
        return tmp->fd;
    }
    else
    {
        SPDLOG_ERROR("Unsupported protocol: {}", ts->protocol);
        return -1;
    }
}

ssize_t TcpServerManager::nsend(int sockfd, const void *buf, size_t len, __attribute__((unused)) int flags)
{
    ssize_t length = 0;
    TcpStream *ts = TcpTable::getInstance().getTcpStreamByFd(sockfd);
    if (ts == nullptr)
    {
        SPDLOG_ERROR("Couldn't found TCP Stream. sockfd:{}", sockfd);
        return -1;
    }

    struct TcpFragment *fragment = (struct TcpFragment *)rte_malloc("TcpFragment", sizeof(struct TcpFragment), 0);
    if (fragment == nullptr)
    {
        SPDLOG_ERROR("Failed to allocate memory for TCP fragment");
        return -2;
    }

    memset(fragment, 0, sizeof(struct TcpFragment));

    fragment->dstPort = ts->srcPort;
    fragment->srcPort = ts->dstPort;

    fragment->acknum = ts->rcvNxt;
    fragment->seqnum = ts->sndNxt;
    SPDLOG_INFO("debug");
    SPDLOG_INFO("fragment->acknum: {}, fragment->seqnum: {}", fragment->acknum, fragment->seqnum);

    fragment->tcp_flags = RTE_TCP_ACK_FLAG | RTE_TCP_PSH_FLAG;
    fragment->windows = TCP_INITIAL_WINDOW;
    fragment->hdrlen_off = 0x50;

    fragment->data = (unsigned char *)rte_malloc("unsigned char *", len + 1, 0);
    if (fragment->data == nullptr)
    {
        SPDLOG_ERROR("Failed to allocate memory for TCP fragment data");
        rte_free(fragment);
        return -1;
    }
    memset(fragment->data, 0, len + 1);
    rte_memcpy(fragment->data, buf, len);
    fragment->length = len;
    length = fragment->length;
    rte_ring_mp_enqueue(ts->sndbuf, fragment);
    return length;
}

int TcpServerManager::nclose(int fd)
{
    struct TcpStream *ts = TcpTable::getInstance().getTcpStreamByFd(fd);
    if (ts == nullptr)
    {
        SPDLOG_INFO("Couldn't found fd:{}", fd);
    }

    if (ts->status != TCP_STATUS::TCP_STATUS_LISTEN)
    {
        struct TcpFragment *fragment = (struct TcpFragment *)rte_malloc("ng_tcp_fragment", sizeof(struct TcpFragment), 0);
        if (fragment == NULL)
            return -1;

        SPDLOG_INFO("nclose --> enter last ack");
        fragment->data = NULL;
        fragment->length = 0;
        fragment->srcPort = ts->dstPort;
        fragment->dstPort = ts->srcPort;

        fragment->seqnum = ts->sndNxt;
        fragment->acknum = ts->rcvNxt;

        fragment->tcp_flags = RTE_TCP_FIN_FLAG | RTE_TCP_ACK_FLAG;
        fragment->windows = TCP_INITIAL_WINDOW;
        fragment->hdrlen_off = 0x50;

        rte_ring_mp_enqueue(ts->sndbuf, fragment);
        ts->status = TCP_STATUS::TCP_STATUS_LAST_ACK;

        freeFdFromBitMap(fd);
    }
    else
    {
        TcpTable::getInstance().removeStream(ts);
        rte_free(ts);
    }

    return 0;
}

int TcpTable::addTcpStream(TcpStream *ts)
{
    if (ts == nullptr)
        return -1;
    std::lock_guard<std::mutex> lock(_mutex);
    _tcpStreamList.emplace_back(ts);
    _count++;
    return 0;
}

TcpStream *TcpTable::getTcpStreamByPort(uint16_t port)
{
    struct TcpStream *ts = nullptr;
    for (auto &it : _tcpStreamList)
    {
        // 只取出半连接的队列
        if (it->dstPort == port && it->fd == -1)
        {
            ts = it;
            return ts;
        }
    }
    return nullptr;
}

TcpStream *TcpTable::getTcpStream(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport)
{
    struct TcpStream *ts = nullptr;
    std::lock_guard<std::mutex> lock(_mutex); 
    for (auto &it : _tcpStreamList)
    {
        if (it->srcIp == sip && it->dstIp == dip && it->srcPort == sport && it->dstPort == dport)
        {
            SPDLOG_INFO("Found established TCP stream: fd: {}, srcIp: {}, dstIp: {}, srcPort: {}, dstPort: {}, status: {}",
                        it->fd, convert_uint32_to_ip(it->srcIp), convert_uint32_to_ip(it->dstIp),
                        ntohs(it->srcPort), ntohs(it->dstPort), (int)it->status);
            ts = it;
            return ts;
        }
    }
    for (auto &it : _tcpStreamList)
    {
        if (it->dstPort == dport && it->status == TCP_STATUS::TCP_STATUS_LISTEN)
        {
            SPDLOG_INFO("Found listening TCP stream: fd: {}, srcIp: {}, dstIp: {}, srcPort: {}, dstPort: {}, status: {}",
                        it->fd, convert_uint32_to_ip(it->srcIp), convert_uint32_to_ip(it->dstIp),
                        ntohs(it->srcPort), ntohs(it->dstPort), (int)it->status);
            ts = it;
            return ts;
        }
    }
    return nullptr;
}

TcpStream *TcpTable::getTcpStreamByFd(int fd)
{
    std::lock_guard<std::mutex> lock(_mutex); 
    for (TcpStream *it : _tcpStreamList)
    {
        if (it->fd == fd)
            return it;
    }
    return nullptr;
}

int TcpTable::removeStream(TcpStream *ts)
{
    std::lock_guard<std::mutex> lock(_mutex); 
    for (TcpStream *it : _tcpStreamList)
    {
        if (it == ts)
            return 1;
    }
    return 0;
}

void TcpTable::debug()
{
    std::lock_guard<std::mutex> lock(_mutex); 
    SPDLOG_INFO("TCP Stream List length: {}", _tcpStreamList.size());
    for (TcpStream *it : _tcpStreamList)
    {
        SPDLOG_INFO("TCP Stream fd: {}, srcIp: {}, dstIp: {}, srcPort: {}, dstPort: {}, status: {}, sndNxt: {}, rcvNxt: {}",
                    it->fd, convert_uint32_to_ip(it->srcIp), convert_uint32_to_ip(it->dstIp),
                    ntohs(it->srcPort), ntohs(it->dstPort), (int)it->status, it->sndNxt, it->rcvNxt);
    }
}