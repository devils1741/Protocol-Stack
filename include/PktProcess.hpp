#ifndef PKT_PROCESS_HPP
#define PKT_PROCESS_HPP
#include <rte_mbuf.h>
#include "Ring.hpp"

struct PktProcessParams
{
    struct rte_mempool *mbufPool;
    struct inout_ring *ring;
};

int pkt_process(void *arg);
#endif