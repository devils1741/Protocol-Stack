#include "dpdkManager.hpp"

DPDKManager::DPDKManager(const string &name, unsigned NUM_MBUFS, int socket_id) : _name(name), _NUM_MBUFS(NUM_MBUFS), _socket_id(socket_id)
{
    SPDLOG_INFO("DPDKManager constructor called");

    _mbufPool = rte_pktmbuf_pool_create(_name.c_str(), _NUM_MBUFS,
                                        0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, _socket_id);
    if (_mbufPool == NULL)
    {
        SPDLOG_ERROR("Could not create mbuf pool");
        rte_exit(EXIT_FAILURE, "Could not create mbuf pool\n");
    }
}

DPDKManager::~DPDKManager()
{
    SPDLOG_INFO("DPDKManager destructor called");
    if (_mbufPool)
    {
        rte_mempool_free(_mbufPool);
        _mbufPool = nullptr;
    }
}

rte_mempool *DPDKManager::getMbufPool() const
{
    return _mbufPool;
}

int DPDKManager::initPort(int portID, rte_eth_conf port_conf_default)
{
    uint16_t nb_sys_ports = rte_eth_dev_count_avail(); //
    if (nb_sys_ports == 0)
    {
        SPDLOG_ERROR("No Supported eth found");
        rte_exit(EXIT_FAILURE, "No Supported eth found\n");
    }

    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(portID, &dev_info); //

    const int num_rx_queues = 1;
    const int num_tx_queues = 1;
    struct rte_eth_conf port_conf = port_conf_default;
    rte_eth_dev_configure(portID, num_rx_queues, num_tx_queues, &port_conf);

    if (rte_eth_rx_queue_setup(portID, 0, 1024,
                               rte_eth_dev_socket_id(portID), NULL, _mbufPool) < 0)
    {
        SPDLOG_ERROR("Could not setup RX queue");
        rte_exit(EXIT_FAILURE, "Could not setup RX queue\n");
    }

    struct rte_eth_txconf txq_conf = dev_info.default_txconf;
    txq_conf.offloads = port_conf.rxmode.offloads;
    if (rte_eth_tx_queue_setup(portID, 0, 1024,
                               rte_eth_dev_socket_id(portID), &txq_conf) < 0)
    {
        SPDLOG_ERROR("Could not setup TX queue");
        rte_exit(EXIT_FAILURE, "Could not setup TX queue\n");
    }

    if (rte_eth_dev_start(portID) < 0)
    {
        SPDLOG_ERROR("Could not start");
        rte_exit(EXIT_FAILURE, "Could not start\n");
    }
    return 0;
}
