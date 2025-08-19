#include "PktProcess.hpp"
#include "ArpProcessor.hpp"
#include "IcmpProcessor.hpp"
#include "UdpProcessor.hpp"
#include "UdpHost.hpp"
#include "Logger.hpp"
#include "ConfigManager.hpp"
#include "TcpHost.hpp"
#include "TcpProcessor.hpp"

int pkt_process(void *arg)
{
    SPDLOG_INFO("Packet processing thread started. Waiting for packets. Current lcore_id={}", rte_lcore_id());
    struct PktProcessParams *pktParams = (struct PktProcessParams *)arg;
    if (pktParams == nullptr)
    {
        SPDLOG_ERROR("Packet processing parameters are null");
        return -1;
    }
    rte_mempool *mbufPool = pktParams->mbufPool;
    struct inout_ring *ring = pktParams->ring;
    if (mbufPool == nullptr || ring == nullptr)
    {
        SPDLOG_ERROR("Mbuf pool or ring is null");
        return -1;
    }
    const int BURST_SIZE = ConfigManager::getInstance().getBurstSize();

    while (1)
    {
        struct rte_mbuf *mbufs[BURST_SIZE];
        unsigned num_recvd = rte_ring_mc_dequeue_burst(ring->in, (void **)mbufs, BURST_SIZE, nullptr);
        unsigned i = 0;
        for (i = 0; i < num_recvd; i++)
        {
            SPDLOG_INFO("Received packet number: {}, current {}", num_recvd, i);
            struct rte_ether_hdr *ehdr = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr *);

            if (ehdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
            {
                SPDLOG_INFO("Received ARP packet ether_type={}", ehdr->ether_type);
                ArpProcessor::getInstance().handlePacket(mbufPool, mbufs[i], ring);
                continue;
            }

            // 不是IPV4协议的包，跳过
            if (ehdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
            {
                continue;
            }

            // 处理IPV4包
            struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbufs[i], struct rte_ipv4_hdr *,
                                                                 sizeof(struct rte_ether_hdr));

            if (iphdr->next_proto_id == IPPROTO_UDP)
            {
                SPDLOG_INFO("Received UDP packet. next_proto_id={}", iphdr->next_proto_id);
                UdpProcessor::getInstance().udpProcess(mbufs[i]);
                UdpProcessor::getInstance().udpOut(mbufPool);
            }

            if (iphdr->next_proto_id == IPPROTO_TCP)
            {
                SPDLOG_INFO("Received TCP packet. next_proto_id={}", iphdr->next_proto_id);
                TcpProcessor::getInstance().tcpProcess(mbufs[i]);
            }

            if (iphdr->next_proto_id == IPPROTO_ICMP)
            {
                SPDLOG_INFO("Received ICMP packet. next_proto_id={}", iphdr->next_proto_id);
                IcmpProcessor::getInstance().handlePacket(mbufPool, mbufs[i], ring);
            }
        }
    }
}

int udp_server(void *arg)
{
    SPDLOG_INFO("UDP server thread started. Current lcore_id={}", rte_lcore_id());
    struct PktProcessParams *pktParams = (struct PktProcessParams *)arg;
    if (pktParams == nullptr)
    {
        SPDLOG_ERROR("Packet processing parameters are null");
        return -1;
    }
    rte_mempool *mbufPool = pktParams->mbufPool;
    if (mbufPool == nullptr)
    {
        SPDLOG_ERROR("Mbuf pool is null");
        return -1;
    }
    return UdpServerManager::getInstance().udpServer(arg);
}

int tcp_server(void *arg)
{
    SPDLOG_INFO("TCP server thread started. Current lcore_id={}", rte_lcore_id());
    struct PktProcessParams *pktParams = (struct PktProcessParams *)arg;
    if (pktParams == nullptr)
    {
        SPDLOG_ERROR("Packet processing parameters are null");
        return -1;
    }
    rte_mempool *mbufPool = pktParams->mbufPool;
    if (mbufPool == nullptr)
    {
        SPDLOG_ERROR("Mbuf pool is null");
        return -1;
    }
    return TcpServerManager::getInstance().tcpServer(arg);
}