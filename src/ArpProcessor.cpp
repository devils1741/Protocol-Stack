#include "ArpProcessor.hpp"
#include "ConfigManager.hpp"
#include "Arp.hpp"
#include "Utils.hpp"

ArpProcessor::ArpProcessor()
{
    SPDLOG_INFO("ArpProcessor initialized");
    // Initialize any other necessary components here
}

ArpProcessor::~ArpProcessor()
{
    SPDLOG_INFO("ArpProcessor destroyed");
    // Initialize any other necessary components here
}

struct rte_mbuf *ArpProcessor::sendArpPacket(struct rte_mempool *mbufPool, uint16_t opcode, uint8_t *srcMac, uint32_t srcIp,
                                             uint8_t *dstMac, uint32_t dstIp)
{
    SPDLOG_INFO("Sending Arp packet: opcode: {}, srcMac: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, srcIp: {}, dstMac: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, dstIp: {}",
                opcode, srcMac[0], srcMac[1], srcMac[2], srcMac[3], srcMac[4], srcMac[5],
                convert_uint32_to_ip(srcIp), dstMac[0], dstMac[1], dstMac[2], dstMac[3], dstMac[4], dstMac[5],
                convert_uint32_to_ip(dstIp));

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

int ArpProcessor::encodeArpPacket(uint8_t *msg, uint16_t opcode, uint8_t *srcMac, uint32_t srcIp,
                                  uint8_t *dstMac, uint32_t dstIp)
{
    // ethernet header
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

    struct rte_arp_hdr *arp = (struct rte_arp_hdr *) (eth + 1);
    arp->arp_hardware = htons(1);
    arp->arp_protocol = htons(RTE_ETHER_TYPE_IPV4);
    arp->arp_hlen = RTE_ETHER_ADDR_LEN;
    arp->arp_plen = sizeof(uint32_t);
    arp->arp_opcode = htons(opcode);

    rte_memcpy(arp->arp_data.arp_sha.addr_bytes, srcMac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(arp->arp_data.arp_tha.addr_bytes, dstMac, RTE_ETHER_ADDR_LEN);

    arp->arp_data.arp_sip = srcIp;
    arp->arp_data.arp_tip = dstIp;

    return 0;
}

int ArpProcessor::setNextProcessor(std::shared_ptr<Processor> nextProcessor)
{
    _nextProcessor = nextProcessor;
    return 0; // Assuming success
}
int ArpProcessor::handlePacket(struct rte_mempool *mbufPool, struct rte_mbuf *mbuf, struct inout_ring *ring)
{
    if (mbuf == nullptr)
    {
        SPDLOG_ERROR("Received null mbuf in ArpProcessor::handlePacket");
        return -1; // Error: mbuf is null
    }

    static uint32_t LOCAL_IP = ConfigManager::getInstance().getLocalAddr();
    auto SRC_MAC = ConfigManager::getInstance().getSrcMac();
    SPDLOG_INFO("LOCAL_IP: {}", convert_uint32_to_ip(LOCAL_IP));

    struct rte_ether_hdr *ehdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_arp_hdr *ahdr = rte_pktmbuf_mtod_offset(mbuf,
                                                       struct rte_arp_hdr *, sizeof(struct rte_ether_hdr));

    SPDLOG_INFO("Received ARP request from packet target IP: {}", convert_uint32_to_ip(ahdr->arp_data.arp_sip));
    if (ehdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
    {
        if (ahdr->arp_data.arp_tip == LOCAL_IP)
        {
            if (ahdr->arp_opcode == rte_cpu_to_be_16(RTE_ARP_OP_REQUEST))
            {

                struct rte_mbuf *arpbuf = sendArpPacket(mbufPool, RTE_ARP_OP_REPLY,
                                                        SRC_MAC, ahdr->arp_data.arp_tip,
                                                        ahdr->arp_data.arp_sha.addr_bytes, ahdr->arp_data.arp_sip);
                rte_ring_mp_enqueue_burst(ring->out, (void **)&arpbuf, 1, NULL);
            }
            else if (ahdr->arp_opcode == rte_cpu_to_be_16(RTE_ARP_OP_REPLY))
            {
                SPDLOG_INFO("Received ARP Replay from IP: {}", convert_uint32_to_ip(ahdr->arp_data.arp_sip));
                uint8_t *hwaddr = ArpTable::search(ahdr->arp_data.arp_sip);
                if (hwaddr == nullptr)
                {
                    ArpHeader arpHeader = {
                        .hardware_type = 0,
                        .sender_protoaddr = ahdr->arp_data.arp_sip,
                    };
                    rte_memcpy(arpHeader.sender_hwaddr, ahdr->arp_data.arp_sha.addr_bytes, RTE_ETHER_ADDR_LEN);
                    SPDLOG_INFO("Adding new ARP entry: IP: {}, MAC: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                                convert_uint32_to_ip(arpHeader.sender_protoaddr),
                                arpHeader.sender_hwaddr[0], arpHeader.sender_hwaddr[1],
                                arpHeader.sender_hwaddr[2], arpHeader.sender_hwaddr[3],
                                arpHeader.sender_hwaddr[4], arpHeader.sender_hwaddr[5]);
                    ArpTable::getInstance().pushBack(arpHeader);
                }
                rte_pktmbuf_free(mbuf);
            }
            else
            {
                // 传递给后续的handler处理
            }
        }
    }

    return 0;
}