#include <iostream>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include "logger.hpp"
#include "configManager.hpp"

int main(int argc, char **argv)
{
    init_logger();
    SPDLOG_INFO("The APP start!");

    ConfigManager &configManager = ConfigManager::getInstance();
    configManager.loadConfig("/repo/Protocol-Stack/config/args.json");
    SPDLOG_INFO("Reading the configuration file...");
    SPDLOG_INFO(configManager.toString());

    const int NUM_MBUFS = configManager.getNumMbufs();
    if (rte_eal_init(argc, argv) < 0)
    {
        SPDLOG_ERROR("Failed to initialize DPDK EAL");
        rte_exit(EXIT_FAILURE, "Error with EAL init\n");
        return -1;
    }

    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("mbuf pool", NUM_MBUFS,
                                                            0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
    {
        SPDLOG_ERROR("Could not create mbuf pool");
        rte_exit(EXIT_FAILURE, "Could not create mbuf pool\n");
    }

    ng_init_port(mbuf_pool);

	rte_eth_macaddr_get(gDpdkPortId, (struct rte_ether_addr *)gSrcMac);
}
