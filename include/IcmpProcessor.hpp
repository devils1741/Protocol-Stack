#ifndef ICMP__HPP
#define ICMP__HPP
#include <rte_memory.h>
#include "Processor.hpp"

class IcmpProcessor : public Processor
{
public:
    int handlePacket(struct rte_mempool *mbufPool, struct rte_mbuf *mbuf, struct inout_ring *ring);
    struct rte_mbuf *sendIcmpPacket(struct rte_mempool *mbufPool, uint8_t *dstMac,
                                    uint32_t sip, uint32_t dip, uint16_t id, uint16_t seqNb);
    int encodeIcmpPkt(uint8_t *msg, uint8_t *dstMac,
                      uint32_t srcIp, uint32_t dstIp, uint16_t id, uint16_t seqNb);
    int setNextProcessor(std::shared_ptr<Processor> nextProcessor);
    uint16_t ng_checksum(uint16_t *addr, int count);

private:
    std::shared_ptr<Processor> _nextProcessor;
};

#endif