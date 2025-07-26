#include "ArpProcess.hpp"

#define MAKE_IPV4_ADDR(a, b, c, d) (a + (b << 8) + (c << 16) + (d << 24))

static uint32_t gLocalIp = MAKE_IPV4_ADDR(192, 168, 0, 104);

ArpProcess::ArpProcess()
{
    SPDLOG_INFO("ArpProcess initialized");
    // Initialize any other necessary components here
}

ArpProcess::~ArpProcess()
{
    SPDLOG_INFO("ArpProcess destroyed");
    // Initialize any other necessary components here
}

struct rte_mbuf *ArpProcess::sendArpPacket(struct rte_mempool *mbufPool, uint16_t opcode, uint8_t *srcMac, uint32_t srcIp,
                                           uint8_t *dstMac, uint32_t dstIp)
{
    SPDLOG_INFO("Sending Arp packet with opcode: {}, srcMac: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, srcIp: {}, dstMac: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, dstIp: {}",
                opcode, srcMac[0], srcMac[1], srcMac[2], srcMac[3], srcMac[4], srcMac[5],
                srcIp, dstMac[0], dstMac[1], dstMac[2], dstMac[3], dstMac[4], dstMac[5],
                dstIp);

    const unsigned total_length = sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr);
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbufPool);
    if (!mbuf)
    {
        SPDLOG_ERROR("mbuf is nullptr, rte_pktmbuf_alloc failed");
        rte_exit(EXIT_FAILURE, "ng_send_arp rte_pktmbuf_alloc\n");
    }

    mbuf->pkt_len = total_length;
    mbuf->data_len = total_length;

    uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);
    encodeArpPacket(pkt_data, opcode, srcMac, srcIp, dstMac, dstIp);
    return mbuf;
}

int ArpProcess::encodeArpPacket(uint8_t *msg, uint16_t opcode, uint8_t *srcMac, uint32_t srcIp,
                                uint8_t *dstMac, uint32_t dstIp)
{
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->s_addr.addr_bytes, srcMac, RTE_ETHER_ADDR_LEN);
    if (!strncmp((const char *)dstMac, (const char *)defaultArpMac, RTE_ETHER_ADDR_LEN))
    {
        SPDLOG_INFO("Encoding Arp Packet. Destination MAC address is broadcast!");
        uint8_t mac[RTE_ETHER_ADDR_LEN] = {0x0};
        rte_memcpy(eth->d_addr.addr_bytes, mac, RTE_ETHER_ADDR_LEN);
    }
    else
    {
        SPDLOG_INFO("Encoding Arp Packet. Destination MAC address: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                    dstMac[0], dstMac[1], dstMac[2], dstMac[3], dstMac[4], dstMac[5]);
        rte_memcpy(eth->d_addr.addr_bytes, dstMac, RTE_ETHER_ADDR_LEN);
    }
    eth->ether_type = htons(RTE_ETHER_TYPE_ARP);
    return 0;
}

int ArpProcess::setNextProcessor(std::shared_ptr<Processor> nextProcessor)
{
    _nextProcessor = nextProcessor;
    return 0; // Assuming success
}
int ArpProcess::handlePacket(struct rte_mempool *mbufPool, struct rte_mbuf *mbuf, struct inout_ring *ring)
{
    if (mbuf == nullptr)
    {
        SPDLOG_ERROR("Received null mbuf in ArpProcess::handlePacket");
        return -1; // Error: mbuf is null
    }
    struct rte_ether_hdr *ehdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_arp_hdr *ahdr = rte_pktmbuf_mtod_offset(mbuf,
                                                       struct rte_arp_hdr *, sizeof(struct rte_ether_hdr));

    if (ehdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
    {
        if (ahdr->arp_data.arp_tip != gLocalIp)
        {
            SPDLOG_INFO("Received ARP request or reply for local IP: {}", gLocalIp);
            struct rte_arp_hdr *ahdr = rte_pktmbuf_mtod_offset(mbuf,
                                                               struct rte_arp_hdr *, sizeof(struct rte_ether_hdr));
            struct rte_mbuf *arpbuf = sendArpPacket(mbufPool, RTE_ARP_OP_REPLY,
                                                    ahdr->arp_data.arp_sha.addr_bytes, ahdr->arp_data.arp_tip,
                                                    ahdr->arp_data.arp_tha.addr_bytes, ahdr->arp_data.arp_sip);
            rte_ring_mp_enqueue_burst(ring->out, (void **)&arpbuf, 1, NULL);
        }
        else
        {
            SPDLOG_INFO("Received ARP response or reply");
        }
    }

    return 0;
}