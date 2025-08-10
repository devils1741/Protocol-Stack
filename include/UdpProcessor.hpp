#ifndef UDP__PROCESSOR
#define UDP__PROCESSOR
#include "BaseNetwork.hpp"
#include <arpa/inet.h>
#include "Processor.hpp"
#include <mutex>

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

struct UdpHost
{
    int fd;                               ///< 文件描述符
    uint32_t localIp;                     ///< 本地IP地址
    uint8_t localMac[RTE_ETHER_ADDR_LEN]; ///< 本地MAC地址
    uint16_t localport;                   ///< 本地端口号
    uint8_t protocal;                     ///< 协议类型
    struct rte_ring *sndbuf;              ///< 发送缓冲区
    struct rte_ring *rcvbuf;              ///< 接收缓冲区
    pthread_cond_t cond;                  ///< 条件变量，用于线程间同步
    pthread_mutex_t mutex;                ///< 互斥锁，用于保护条件变量
};

class UdpProcessor : public BaseNetwork
{
public:
    static UdpProcessor &getInstance()
    {
        static UdpProcessor instance;
        return instance;
    }
    int createSocket(__attribute__((unused)) int domain, int type, __attribute__((unused)) int protocol);
    int nbind(int sockfd, const struct sockaddr *addr, __attribute__((unused)) socklen_t addrlen);
    int udpProcess(struct rte_mbuf *udpMbuf);
    int udpOut(struct rte_mempool *mbuf_pool);
    struct UdpHost *getHostInfoFromIpAndPort(uint32_t dip, uint16_t port, uint8_t proto);
    struct rte_mbuf *udpPkt(struct rte_mempool *mbuf_pool, uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort, uint8_t *srcMac, uint8_t *dstMac, uint8_t *data, uint16_t length);
    int encodeUdpApppkt(uint8_t *msg, uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort, uint8_t *srcMac, uint8_t *dstMac, unsigned char *data, uint16_t total_len);

private:
    UdpProcessor() = default;
    ~UdpProcessor() = default;
    UdpProcessor(const UdpProcessor &) = delete;
    UdpProcessor &operator=(const UdpProcessor &) = delete;
    UdpProcessor(UdpProcessor &&) = delete;
    UdpProcessor &operator=(UdpProcessor &&) = delete;

private:
    static std::list<UdpHost *> _udpHostList;  ///< 本地主机信息
    std::mutex _mutex;                         ///< 互斥锁，用于保护_udpHostList
    std::shared_ptr<Processor> _nextProcessor; ///< 下一个处理器
};

#endif