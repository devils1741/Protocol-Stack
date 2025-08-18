#include "IcmpProcessor.hpp"
#include <rte_ether.h>
#include <rte_ip.h>
#include <arpa/inet.h>
#include <rte_icmp.h>
#include "Logger.hpp"
#include "Ring.hpp"
#include "ConfigManager.hpp"
#include "Utils.hpp"
int IcmpProcessor::handlePacket(struct rte_mempool *mbufPool, struct rte_mbuf *mbuf, struct inout_ring *ring)
{
    struct rte_ether_hdr *ehdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
                                                         sizeof(struct rte_ether_hdr));

    if (iphdr->next_proto_id == IPPROTO_ICMP)
    {
        struct rte_icmp_hdr *icmphdr = (struct rte_icmp_hdr *)(iphdr + 1);

        if (icmphdr->icmp_type == RTE_IP_ICMP_ECHO_REQUEST)
        {
            uint16_t icmp_len = ntohs(iphdr->total_length) - sizeof(struct rte_ipv4_hdr);
            uint8_t *icmp_data = (uint8_t *)icmphdr;

            // 构造回复包
            struct rte_mbuf *txbuf = sendIcmpPacket(mbufPool,
                                                    ehdr->s_addr.addr_bytes,
                                                    iphdr->dst_addr,
                                                    iphdr->src_addr,
                                                    icmphdr->icmp_ident,
                                                    icmphdr->icmp_seq_nb,
                                                    icmp_data,
                                                    icmp_len);

            rte_ring_mp_enqueue_burst(ring->out, (void **)&txbuf, 1, nullptr);
            rte_pktmbuf_free(mbuf);
        }
    }
    return 0;
}

struct rte_mbuf *IcmpProcessor::sendIcmpPacket(struct rte_mempool *mbufPool, uint8_t *dstMac,
                                               uint32_t sip, uint32_t dip,
                                               uint16_t id, uint16_t seqNb,
                                               uint8_t *payload, uint16_t payload_len)
{
    const unsigned totalLength = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + payload_len;
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbufPool);
    if (mbuf == nullptr)
    {
        SPDLOG_ERROR("IcmpProcessor::sendIcmpPacket: Failed to allocate mbuf");
        return nullptr;
    }
    mbuf->pkt_len = totalLength;
    mbuf->data_len = totalLength;
    uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);

    encodeIcmpPkt(pkt_data, dstMac, sip, dip, id, seqNb, payload, payload_len);
    return mbuf;
}

int IcmpProcessor::encodeIcmpPkt(uint8_t *msg, uint8_t *dstMac,
                                 uint32_t srcIp, uint32_t dstIp,
                                 uint16_t id, uint16_t seqNb,
                                 uint8_t *payload, uint16_t payload_len)
{
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    auto SRC_MAC = ConfigManager::getInstance().getSrcMac();
    rte_memcpy(eth->s_addr.addr_bytes, SRC_MAC, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->d_addr.addr_bytes, dstMac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(msg + sizeof(struct rte_ether_hdr));
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(sizeof(struct rte_ipv4_hdr) + payload_len);
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_ICMP;
    ip->src_addr = srcIp;
    ip->dst_addr = dstIp;
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    struct rte_icmp_hdr *icmp = (struct rte_icmp_hdr *)(msg + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
    rte_memcpy(icmp, payload, payload_len); // 直接拷贝原来的整个 ICMP 包（头+数据）
    icmp->icmp_type = RTE_IP_ICMP_ECHO_REPLY;
    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = ng_checksum((uint16_t *)icmp, payload_len);

    return 0;
}

uint16_t IcmpProcessor::ng_checksum(uint16_t *addr, int count)
{
    register long sum = 0;
    while (count > 1)
    {

        sum += *(unsigned short *)addr++;
        count -= 2;
    }
    if (count > 0)
    {
        sum += *(unsigned char *)addr;
    }
    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}

int IcmpProcessor::setNextProcessor(std::shared_ptr<Processor> nextProcessor)
{
    return 0;
}