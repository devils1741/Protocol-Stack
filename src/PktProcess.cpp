#include "PktProcess.hpp"
#include <rte_ethdev.h>

int BURST_SIZE = 32;

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

    while (1)
    {
        struct rte_mbuf *mbufs[BURST_SIZE];
        unsigned num_recvd = rte_ring_mc_dequeue_burst(ring->in, (void **)mbufs, BURST_SIZE, NULL);
        unsigned i = 0;
        for (i = 0; i < num_recvd; i++)
        {
            SPDLOG_INFO("Received packet");
            struct rte_ether_hdr *ehdr = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr *);
            if (ehdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
            {
                SPDLOG_INFO("Received ARP packet");
            }
        }
    }
}