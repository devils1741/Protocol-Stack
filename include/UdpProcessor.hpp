#ifndef __UDP__PROCESSOR__
#define __UDP__PROCESSOR__
#include "BaseNetwork.hpp"
#include <arpa/inet.h>
#include "Processor.hpp"
#include "UdpHost.hpp"
#include <mutex>


class UdpProcessor
{
public:
    static UdpProcessor &getInstance()
    {
        static UdpProcessor instance;
        return instance;
    }
    int udpProcess(struct rte_mbuf *udpMbuf);
    int udpOut(struct rte_mempool *mbuf_pool);
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
    std::shared_ptr<Processor> _nextProcessor; ///< 下一个处理器
};

#endif