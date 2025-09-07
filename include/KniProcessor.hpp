#ifndef KNI_PROCESSOR__HPP
#define KNI_PROCESSOR__HPP
#include <rte_kni.h>
#include <rte_mbuf.h> 


class KniProcessor
{
public:
    static KniProcessor &getInstance()
    {
        static KniProcessor instance;
        return instance;
    }
    int setKni(struct rte_kni *kni);
    int burstTx(rte_mbuf **mbufs,unsigned numRecvd);
    int kniHandleRequests();
private:
    struct rte_kni *_kni = nullptr;
};

#endif
