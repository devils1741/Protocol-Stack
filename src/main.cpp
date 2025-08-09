#include <iostream>
#include <memory>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include "Logger.hpp"
#include "ConfigManager.hpp"
#include "DpdkManager.hpp"
#include "Ring.hpp"
#include "PktProcess.hpp"

#include "Arp.hpp"

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_rx_pkt_len = RTE_ETHER_MAX_LEN}};

int main(int argc, char **argv)
{
    initLogger();
    SPDLOG_INFO("The APP start!");

    ConfigManager &configManager = ConfigManager::getInstance();
    configManager.loadConfig("/repo/Protocol-Stack/config/args.json");
    SPDLOG_INFO("Reading the configuration file...");
    SPDLOG_INFO(configManager.toString());

    const int NUM_MBUFS = configManager.getNumMbufs();
    const int DPDK_PORT_ID = configManager.getDpdkPortId();
    const int RING_SIZE = configManager.getRingSize();
    const int BURST_SIZE = configManager.getBurstSize();
    // const uint32_t LOCAL_ADDR = configManager.getLocalAddr();

    // ArpTable::getInstance();

    if (rte_eal_init(argc, argv) < 0)
    {
        SPDLOG_ERROR("Failed to initialize DPDK EAL");
        rte_exit(EXIT_FAILURE, "Error with EAL init\n");
        return -1;
    }

    std::shared_ptr<DPDKManager> dpdkManager = std::make_shared<DPDKManager>("mbuf pool", NUM_MBUFS, rte_socket_id());
    if (dpdkManager->initPort(DPDK_PORT_ID, port_conf_default) < 0)
    {
        SPDLOG_ERROR("Failed to initialize DPDK port");
        rte_exit(EXIT_FAILURE, "Error with port init\n");
        return -1;
    }

    if (rte_eth_macaddr_get(DPDK_PORT_ID, (struct rte_ether_addr *)configManager.getSrcMac()) == 0)
    {
        auto SRC_MAC = configManager.getSrcMac();
        SPDLOG_INFO("Source MAC address: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                    SRC_MAC[0], SRC_MAC[1], SRC_MAC[2], SRC_MAC[3], SRC_MAC[4], SRC_MAC[5]);
    }
    else
    {
        SPDLOG_ERROR("Failed to get source MAC address");
        rte_exit(EXIT_FAILURE, "Error getting MAC address\n");
        return -1;
    }

    Ring::getSingleton().setRingSize(RING_SIZE);
    struct inout_ring *ring = Ring::getSingleton().getRing();

    unsigned lcore_id = rte_lcore_id();
    lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
    struct PktProcessParams pktParams = {
        .mbufPool = dpdkManager->getMbufPool(),
        .ring = ring};
    rte_eal_remote_launch(pkt_process, &pktParams, lcore_id);

    // 设置接收队列和发送队列
    while (1)
    {
        // 接收数据包
        struct rte_mbuf *rx[BURST_SIZE];
        unsigned num_recvd = rte_eth_rx_burst(DPDK_PORT_ID, 0, rx, BURST_SIZE);
        if (num_recvd > BURST_SIZE)
        {
            SPDLOG_ERROR("Received more packets than burst size");
            rte_exit(EXIT_FAILURE, "Received more packets than burst size\n");
        }
        else if (num_recvd > 0)
        {
            rte_ring_sp_enqueue_burst(ring->in, (void **)rx, num_recvd, NULL);
            SPDLOG_INFO("Received {} packets from port {}", num_recvd, DPDK_PORT_ID);
        }
        // 发送数据包
        struct rte_mbuf *tx[BURST_SIZE];
        unsigned nb_tx = rte_ring_sc_dequeue_burst(ring->out, (void **)tx, BURST_SIZE, NULL);
        if (nb_tx > 0)
        {
            SPDLOG_INFO("Send packets with port {} ", DPDK_PORT_ID);
            rte_eth_tx_burst(DPDK_PORT_ID, 0, tx, nb_tx);
            unsigned i = 0;
            for (i = 0; i < nb_tx; i++)
            {
                rte_pktmbuf_free(tx[i]);
            }
        }
    }

    rte_eal_wait_lcore(lcore_id);
}
