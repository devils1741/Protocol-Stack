#ifndef ARP_PROCESS_HPP
#define ARP_PROCESS_HPP
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ethdev.h>
#include <arpa/inet.h>

#include "Processor.hpp"
#include "Arp.hpp"
#include "Logger.hpp"
#include "Ring.hpp"
#include "PktProcess.hpp"

class ArpProcessor : public Processor
{
public:
    ArpProcessor();
    ~ArpProcessor();

    struct rte_mbuf *sendArpPacket(struct rte_mempool *mbuf_pool, uint16_t opcode, uint8_t *srcMac, uint32_t srcIp,
                                   uint8_t *dstMac, uint32_t dstIp);
    int handlePacket(struct rte_mempool *mbufPool, struct rte_mbuf *mbufs, struct inout_ring *ring);
    void getDefaultArpMac(uint8_t *copy);
    /**
     * @brief 编码arp包到msg中
     * @param msg 指向要编码的缓冲区
     * @param opcode arp操作码,如ARP请求或ARP应答
     * @param srcMac 源MAC地址
     * @param srcIp 源IP地址
     * @param dstMac 目标MAC地址
     * @param dstIp 目标IP地址
     * @return 成功时返回0
     */
    int encodeArpPacket(uint8_t *msg, uint16_t opcode, uint8_t *srcMac, uint32_t srcIp,
                        uint8_t *dstMac, uint32_t dstIp);

protected:
    int setNextProcessor(std::shared_ptr<Processor> nextProcessor) override;

private:
    const uint8_t defaultArpMac[RTE_ETHER_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; ///< 默认的广播MAC地址
    std::shared_ptr<Processor> _nextProcessor;
};

#endif