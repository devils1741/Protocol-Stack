#ifndef ICMP__HPP
#define ICMP__HPP
#include <rte_memory.h>
#include "Processor.hpp"

class IcmpProcessor : public Processor
{
public:
    static IcmpProcessor &getInstance()
    {
        static IcmpProcessor instance;
        return instance;
    }
    int handlePacket(struct rte_mempool *mbufPool, struct rte_mbuf *mbuf, struct inout_ring *ring);
    struct rte_mbuf *sendIcmpPacket(struct rte_mempool *mbufPool, uint8_t *dstMac,
                                    uint32_t sip, uint32_t dip, uint16_t id, uint16_t seqNb, uint8_t *payload, uint16_t payload_len);
    int encodeIcmpPkt(uint8_t *msg, uint8_t *dstMac,
                      uint32_t srcIp, uint32_t dstIp, uint16_t id, uint16_t seqNb, uint8_t *payload, uint16_t payload_len);
    int setNextProcessor(std::shared_ptr<Processor> nextProcessor);
    uint16_t ng_checksum(uint16_t *addr, int count);

private:
    IcmpProcessor() = default;
    ~IcmpProcessor() = default;
    IcmpProcessor(const IcmpProcessor &) = delete;
    IcmpProcessor &operator=(const IcmpProcessor &) = delete;
    IcmpProcessor(IcmpProcessor &&) = delete;
    IcmpProcessor &operator=(IcmpProcessor &&) = delete;

private:
    std::shared_ptr<Processor> _nextProcessor;
};

#endif