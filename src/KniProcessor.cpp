#include <KniProcessor.hpp>
#include "Logger.hpp"


int KniProcessor::setKni(struct rte_kni *kni)
{
    _kni = kni;
    return 0;
}

int KniProcessor::burstTx(rte_mbuf **mbufs,unsigned numRecvd)
{
    unsigned count = rte_kni_tx_burst(_kni, mbufs, numRecvd);
    SPDLOG_INFO("KNI burstTx send {} packets", count);
    return 0;
}


int KniProcessor::kniHandleRequests()
{
    rte_kni_handle_request(_kni);
}