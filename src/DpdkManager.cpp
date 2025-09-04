#include "DpdkManager.hpp"
#include "ConfigManager.hpp"
#include <rte_errno.h>

DPDKManager::DPDKManager(const string &name, unsigned NUM_MBUFS, int socket_id) : _name(name), _NUM_MBUFS(NUM_MBUFS), _socket_id(socket_id)
{
    SPDLOG_INFO("DPDKManager constructor called");

    _mbufPool = rte_pktmbuf_pool_create(_name.c_str(), _NUM_MBUFS,
                                        0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, _socket_id);
    if (_mbufPool == nullptr)
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
    SPDLOG_INFO("DPDK Port Initialization started for port ID: {}", portID);
    // 确认系统里至少有1个可用的以太网端口
    uint16_t nb_sys_ports = rte_eth_dev_count_avail(); //
    if (nb_sys_ports == 0)
    {
        SPDLOG_ERROR("No Supported eth found");
        rte_exit(EXIT_FAILURE, "No Supported eth found\n");
    }
    SPDLOG_INFO("Number of available ports: {}", nb_sys_ports);
    // 获取指定端口的以太网信息
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(portID, &dev_info); 
    // 接收队列数量
    const int num_rx_queues = 1;
    // 发送队列数量
    const int num_tx_queues = 1;
    // 端口配置信息
    struct rte_eth_conf port_conf = port_conf_default;
    rte_eth_dev_configure(portID, num_rx_queues, num_tx_queues, &port_conf);
    //设置接收队列
    if (rte_eth_rx_queue_setup(portID, 0, 1024,
                               rte_eth_dev_socket_id(portID), nullptr, _mbufPool) < 0)
    {
        SPDLOG_ERROR("Could not setup RX queue");
        rte_exit(EXIT_FAILURE, "Could not setup RX queue\n");
    }
    // 设置发送队列
    struct rte_eth_txconf txq_conf = dev_info.default_txconf;
    txq_conf.offloads = port_conf.rxmode.offloads;
    if (rte_eth_tx_queue_setup(portID, 0, 1024,
                               rte_eth_dev_socket_id(portID), &txq_conf) < 0)
    {
        SPDLOG_ERROR("Could not setup TX queue");
        rte_exit(EXIT_FAILURE, "Could not setup TX queue\n");
    }
    // 启动网卡
    if (rte_eth_dev_start(portID) < 0)
    {
        SPDLOG_ERROR("Could not start port {}", portID);
        rte_exit(EXIT_FAILURE, "Could not start\n");
    }

    if(ConfigManager::getInstance().isKniEnabled())
    {
        rte_eth_promiscuous_enable(portID);
    }
    return 0;
}



struct rte_kni* DPDKManager::allocKni(int portID)
{
    struct rte_kni *kniHandler = nullptr;
    struct rte_kni_conf kniConf;
    memset(&kniConf, 0, sizeof(kniConf));
    SPDLOG_INFO("KNI Port Initialization started for port ID: {}", portID);
    snprintf(kniConf.name, RTE_KNI_NAMESIZE, "vEth%u", portID);
    kniConf.group_id = portID;
    kniConf.mbuf_size = ConfigManager::getInstance().getMaxPacketSize();
    rte_eth_macaddr_get(portID, (struct rte_ether_addr *)kniConf.mac_addr);
    rte_eth_dev_get_mtu(portID, &kniConf.mtu);

    struct rte_kni_ops ops;
    memset(&ops, 0, sizeof(ops));
    ops.port_id = portID;
    ops.config_network_if = configNetworkIf;

    kniHandler = rte_kni_alloc(_mbufPool, &kniConf, &ops);
    if (kniHandler == nullptr)
    {
        SPDLOG_ERROR("Failed to create KNI. {}. Error code {}", rte_strerror(rte_errno), rte_errno);
        rte_exit(EXIT_FAILURE, "Failed to create KNI\n");
    }
    return kniHandler;
}

int DPDKManager::configNetworkIf(uint16_t portId, uint8_t ifUp)
{
    if(!rte_eth_dev_is_valid_port(portId))
    {
        SPDLOG_ERROR("Invalid port ID: {}", portId);
        return -1;
    }

    int ret = 0;
    if(ifUp)
    {
       rte_eth_dev_stop(portId);
       ret = rte_eth_dev_start(portId);
    }else
    {
        rte_eth_dev_stop(portId);
    }

    if(ret<0)
    {
        SPDLOG_INFO("Failed to {} the port: {}", ifUp ? "start" : "stop", portId);
        return -1;
    }
    return 0;
}