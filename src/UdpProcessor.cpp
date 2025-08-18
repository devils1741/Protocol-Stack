#include "UdpProcessor.hpp"
#include <rte_malloc.h>
#include "ConfigManager.hpp"
#include "Arp.hpp"
#include "Utils.hpp"
#include "ArpProcessor.hpp"
#include "Ring.hpp"
#include "UdpHost.hpp"


int UdpProcessor::udpProcess(struct rte_mbuf *udpMbuf)
{
    struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(udpMbuf, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
    struct rte_udp_hdr *udphdr = (struct rte_udp_hdr *)(iphdr + 1);

    struct in_addr addr;
    addr.s_addr = iphdr->src_addr;
    SPDLOG_INFO("UDP Processing Packet ---> src: {}, dst: {}, src_port: {}, dst_port: {}",
                inet_ntoa(addr), convert_uint32_to_ip(iphdr->dst_addr), ntohs(udphdr->src_port), ntohs(udphdr->dst_port));

    struct UdpHost *host = UdpServerManager::getInstance().getHostInfoFromIpAndPort(iphdr->dst_addr, udphdr->dst_port, iphdr->next_proto_id);
    if (host == nullptr)
    {
        SPDLOG_INFO("UDP host not found for IP: {}, Port: {}",
                    convert_uint32_to_ip(iphdr->dst_addr), ntohs(udphdr->dst_port));
        rte_pktmbuf_free(udpMbuf);
        return -3;
    }

    struct offload *ol = (struct offload *)rte_malloc("offload", sizeof(struct offload), 0);
    if (ol == nullptr)
    {
        SPDLOG_INFO("Failed to allocate offload structure for UDP processing");
        rte_pktmbuf_free(udpMbuf);
        return -1;
    }

    ol->dip = iphdr->dst_addr;
    ol->sip = iphdr->src_addr;
    ol->sport = udphdr->src_port;
    ol->dport = udphdr->dst_port;

    ol->protocol = IPPROTO_UDP;
    ol->length = ntohs(udphdr->dgram_len);

    ol->data = (unsigned char *)rte_malloc("unsigned char*", ol->length - sizeof(struct rte_udp_hdr), 0);
    if (ol->data == nullptr)
    {
        rte_pktmbuf_free(udpMbuf);
        rte_free(ol);
        return -2;
    }
    rte_memcpy(ol->data, (unsigned char *)(udphdr + 1), ol->length - sizeof(struct rte_udp_hdr));

    rte_ring_mp_enqueue(host->rcvbuf, ol); // recv buffer

    pthread_mutex_lock(&host->mutex);
    pthread_cond_signal(&host->cond);
    pthread_mutex_unlock(&host->mutex);

    rte_pktmbuf_free(udpMbuf);

    return 0;
}

int UdpProcessor::udpOut(struct rte_mempool *mbuf_pool)
{
    struct inout_ring *ring = Ring::getSingleton().getRing();
    std::list<UdpHost*> _udpHostList = UdpServerManager::getInstance().getUdpHostList();
    for (auto &host : _udpHostList)
    {
        struct offload *ol;
        int nbSnd = rte_ring_mc_dequeue(host->sndbuf, (void **)&ol);
        if (nbSnd < 0)
            continue;

        uint8_t *dstMac = ArpTable::getInstance().search(ol->dip);
        if (dstMac == nullptr)
        {
            uint8_t *dstMac = new uint8_t[RTE_ETHER_ADDR_LEN];
            SPDLOG_INFO("MAC not found for IP: {}, Port: {}",
                        convert_uint32_to_ip(ol->dip), ntohs(ol->dport));
            ArpProcessor::getInstance().getDefaultArpMac(dstMac);
            struct rte_mbuf *arpBuf = ArpProcessor::getInstance().sendArpPacket(mbuf_pool, RTE_ARP_OP_REQUEST,
                                                                 ConfigManager::getInstance().getSrcMac(), ol->sip,
                                                                 dstMac, ol->dip);
            rte_ring_mp_enqueue_burst(ring->out, (void **)&arpBuf, 1, nullptr);
            rte_ring_mp_enqueue(host->sndbuf, ol);
        }
        else
        {
            struct rte_mbuf *udpbuf = udpPkt(mbuf_pool, ol->sip, ol->dip, ol->sport, ol->dport,
                                             host->localMac, dstMac, ol->data, ol->length);
            rte_ring_mp_enqueue_burst(ring->out, (void **)&udpbuf, 1, nullptr);
        }
    }

    return 0;
}

struct rte_mbuf *UdpProcessor::udpPkt(struct rte_mempool *mbuf_pool, uint32_t srcIp, uint32_t dstIp,
                                      uint16_t srcPort, uint16_t dstPort, uint8_t *srcMac, uint8_t *dstMac,
                                      uint8_t *data, uint16_t length)
{

    const unsigned total_len = length + 42;

    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!mbuf)
    {
        rte_exit(EXIT_FAILURE, "rte_pktmbuf_alloc\n");
    }
    mbuf->pkt_len = total_len;
    mbuf->data_len = total_len;

    uint8_t *pktdata = rte_pktmbuf_mtod(mbuf, uint8_t *);

    encodeUdpApppkt(pktdata, srcIp, dstIp, srcPort, dstPort, srcMac, dstMac, data, total_len);

    return mbuf;
}

int UdpProcessor::encodeUdpApppkt(uint8_t *msg, uint32_t srcIp, uint32_t dstIp,
                                  uint16_t srcPort, uint16_t dstPort, uint8_t *srcMac, uint8_t *dstMac,
                                  unsigned char *data, uint16_t total_len)
{
    SPDLOG_INFO("encodeUdpApppkt: sIp: {}, dIp: {}, sPort: {}, dPort: {}, total_len: {}",
                convert_uint32_to_ip(srcIp), convert_uint32_to_ip(dstIp), ntohs(srcPort), ntohs(dstPort), total_len);

    // 1 ethhdr
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->s_addr.addr_bytes, srcMac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->d_addr.addr_bytes, dstMac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    // 2 iphdr
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(msg + sizeof(struct rte_ether_hdr));
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64; // ttl = 64
    ip->next_proto_id = IPPROTO_UDP;
    ip->src_addr = srcIp;
    ip->dst_addr = dstIp;
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // 3 udphdr
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(msg + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
    udp->src_port = srcPort;
    udp->dst_port = dstPort;
    uint16_t udplen = total_len - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr);
    udp->dgram_len = htons(udplen);

    rte_memcpy((uint8_t *)(udp + 1), data, udplen);

    udp->dgram_cksum = 0;
    udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);

    return 0;
}
