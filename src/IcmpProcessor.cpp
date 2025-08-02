#include "IcmpProcessor.hpp"
#include <rte_ether.h>
#include <rte_ip.h>
#include <arpa/inet.h>
#include <rte_icmp.h>
#include "Logger.hpp"
#include "Ring.hpp"

int IcmpProcessor::handlePacket(struct rte_mempool *mbufPool, struct rte_mbuf *mbuf, struct inout_ring *ring)
{
    struct rte_ether_hdr *ehdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_arp_hdr *ahdr = rte_pktmbuf_mtod_offset(mbuf,
                                                       struct rte_arp_hdr *, sizeof(struct rte_ether_hdr));
    struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
                                                         sizeof(struct rte_ether_hdr));

    if (iphdr->next_proto_id == IPPROTO_ICMP)
    {
        struct rte_icmp_hdr *icmphdr = (struct rte_icmp_hdr *)(iphdr + 1);
        struct in_addr addr;
        addr.s_addr = iphdr->src_addr;
        SPDLOG_INFO("Received ICMP packet from {}", inet_ntoa(addr));

        if (icmphdr->icmp_type == RTE_IP_ICMP_ECHO_REQUEST)
        {
            addr.s_addr = iphdr->dst_addr;
            SPDLOG_INFO("Received ICMP Echo Request from {} to {}", inet_ntoa(addr), inet_ntoa(*(struct in_addr *)&iphdr->dst_addr));
            struct rte_mbuf *txbuf = sendIcmpPacket(mbufPool, ehdr->s_addr.addr_bytes,
                                                    iphdr->dst_addr, iphdr->src_addr, icmphdr->icmp_ident, icmphdr->icmp_seq_nb);
            rte_ring_mp_enqueue_burst(ring->out, (void **)&txbuf, 1, NULL);

            rte_pktmbuf_free(mbuf);
        }
    }
}

struct rte_mbuf *IcmpProcessor::sendIcmpPacket(struct rte_mempool *mbufPool, uint8_t *dstMac,
                                               uint32_t sip, uint32_t dip, uint16_t id, uint16_t seqNb)
{
    SPDLOG_INFO("IcmpProcessor::sendIcmpPacket: Sending ICMP packet from {} to {} with id={} seqNb={}",
                inet_ntoa(*(struct in_addr *)&sip), inet_ntoa(*(struct in_addr *)&dip), id, seqNb);
    const unsigned totalLength = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_icmp_hdr);
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbufPool);
    if (mbuf == nullptr)
    {
        SPDLOG_ERROR("IcmpProcessor::sendIcmpPacket: Failed to allocate mbuf");
        rte_exit(EXIT_FAILURE, "Failed to allocate mbuf\n");
        return nullptr;
    }
    mbuf->pkt_len = totalLength;
    mbuf->data_len = totalLength;
    uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);
    encodeIcmpPkt(pkt_data, dstMac, sip, dip, id, seqNb);
    return mbuf;
}

int IcmpProcessor::encodeIcmpPkt(uint8_t *msg, uint8_t *dstMac,
                                 uint32_t srcIp, uint32_t dstIp, uint16_t id, uint16_t seqNb)
{
    if (msg == nullptr)
    {
        SPDLOG_ERROR("IcmpProcessor::encodeIcmpPkt: msg is null");
        return -1;
    }
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    uint8_t srcMac[RTE_ETHER_ADDR_LEN];
    rte_memcpy(eth->s_addr.addr_bytes, srcMac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->d_addr.addr_bytes, dstMac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    SPDLOG_INFO("encodeIcmpPkt srcMac={:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x} dstMac={:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x} srcIp={} dstIp={} id={} seqNb={}",
                srcMac[0], srcMac[1], srcMac[2], srcMac[3], srcMac[4], srcMac[5],
                dstMac[0], dstMac[1], dstMac[2], dstMac[3], dstMac[4], dstMac[5],
                srcIp, dstIp, id, seqNb);

    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(msg + sizeof(struct rte_ether_hdr));
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_icmp_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64; // ttl = 64
    ip->next_proto_id = IPPROTO_ICMP;
    ip->src_addr = srcIp;
    ip->dst_addr = dstIp;

    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    struct rte_icmp_hdr *icmp = (struct rte_icmp_hdr *)(msg + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
    icmp->icmp_type = RTE_IP_ICMP_ECHO_REPLY;
    icmp->icmp_code = 0;
    icmp->icmp_ident = id;
    icmp->icmp_seq_nb = seqNb;

    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = ng_checksum((uint16_t *)icmp, sizeof(struct rte_icmp_hdr));
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