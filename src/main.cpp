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

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_rx_pkt_len = RTE_ETHER_MAX_LEN}};

static uint8_t gSrcMac[RTE_ETHER_ADDR_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

int main(int argc, char **argv)
{
    initLogger();
    SPDLOG_INFO("The APP start!");

    ConfigManager &configManager = ConfigManager::getInstance();
    configManager.loadConfig("/repo/Protocol-Stack/config/args.json");
    SPDLOG_INFO("Reading the configuration file...");
    SPDLOG_INFO(configManager.toString());

    const int NUM_MBUFS = configManager.getNumMbufs();
    const int G_DPDK_PORT_ID = configManager.getDpdkPortId();
    const int RING_SIZE = configManager.getRingSize();
    const int BURST_SIZE = configManager.getBurstSize();
    const uint32_t LOCAL_ADDR = configManager.getLocalAddr();

    if (rte_eal_init(argc, argv) < 0)
    {
        SPDLOG_ERROR("Failed to initialize DPDK EAL");
        rte_exit(EXIT_FAILURE, "Error with EAL init\n");
        return -1;
    }

    std::shared_ptr<DPDKManager> dpdkManager = std::make_shared<DPDKManager>("mbuf pool", NUM_MBUFS, rte_socket_id());
    if (dpdkManager->initPort(G_DPDK_PORT_ID, port_conf_default) < 0)
    {
        SPDLOG_ERROR("Failed to initialize DPDK port");
        rte_exit(EXIT_FAILURE, "Error with port init\n");
        return -1;
    }

    if (rte_eth_macaddr_get(G_DPDK_PORT_ID, (struct rte_ether_addr *)gSrcMac) == 0)
    {
        SPDLOG_INFO("Source MAC address: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                    gSrcMac[0], gSrcMac[1], gSrcMac[2], gSrcMac[3], gSrcMac[4], gSrcMac[5]);
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
        .ring = ring,
        .BURST_SIZE = BURST_SIZE,
        .LOCAL_ADDR = LOCAL_ADDR};
    rte_eal_remote_launch(pkt_process, &pktParams, lcore_id);

    // 设置接收队列和发送队列
    while (1)
    {
        // 接收数据包
        struct rte_mbuf *rx[BURST_SIZE];
        unsigned num_recvd = rte_eth_rx_burst(G_DPDK_PORT_ID, 0, rx, BURST_SIZE);
        if (num_recvd > BURST_SIZE)
        {
            SPDLOG_ERROR("Received more packets than burst size");
            rte_exit(EXIT_FAILURE, "Received more packets than burst size\n");
        }
        else if (num_recvd > 0)
        {
            rte_ring_sp_enqueue_burst(ring->in, (void **)rx, num_recvd, NULL);
            SPDLOG_INFO("Received {} packets from port {}", num_recvd, G_DPDK_PORT_ID);
        }
    }

    rte_eal_wait_lcore(lcore_id);
}
