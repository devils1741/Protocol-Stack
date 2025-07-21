#ifndef PKT_PROCESS_HPP
#define PKT_PROCESS_HPP
#include <rte_mbuf.h>
#include "Logger.hpp"
#include "Ring.hpp"

struct PktProcessParams
{
    struct rte_mempool *mbufPool;
    struct inout_ring *ring;
    int BURST_SIZE;
};

int pkt_process(void *arg);
#endif