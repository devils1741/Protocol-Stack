#ifndef DPDK_MANAGER_HPP
#define DPDK_MANAGER_HPP
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <memory>
#include <string>
#include "logger.hpp"
using std::string;

class DPDKManager
{
public:
    DPDKManager(rte_mempool &mbufPool);
    ~DPDKManager();
    int setMemPool(rte_mempool &mbufPool);
    int initPort(int portID, rte_eth_conf port_conf_default);

private:
    DPDKManager(const DPDKManager &) = delete;
    DPDKManager(DPDKManager &&) = delete;
    DPDKManager &operator=(const DPDKManager &) = delete;
    DPDKManager &operator=(DPDKManager &&) = delete;
    struct rte_mempool *_mbufPool = nullptr;
};

#endif
