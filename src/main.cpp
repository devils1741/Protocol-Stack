#include <iostream>
#include <memory>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include "logger.hpp"
#include "configManager.hpp"
#include "dpdkManager.hpp"
#include "ring.hpp"

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
    const int RING_SIZE = 1024;
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

    struct inout_ring *ring = Ring::getSingleton().getRing();
    if (ring == NULL)
    {
        SPDLOG_ERROR("Failed to allocate memory for ring buffer");
        rte_exit(EXIT_FAILURE, "ring buffer init failed\n");
    }

    if (ring->in == NULL)
    {
        SPDLOG_INFO("Creating input ring");
        ring->in = rte_ring_create("in ring", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    }
    if (ring->out == NULL)
    {
        SPDLOG_INFO("Creating out ring");
        ring->out = rte_ring_create("out ring", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    }
}
