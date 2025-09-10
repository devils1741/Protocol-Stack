#include "DDosDetect.hpp"
#include "Logger.hpp"
#include <rte_mbuf.h>

double DDosDetect::ddosEntropy(double setBits, double totalBits)
{
    if (totalBits == 0 || setBits == 0 || setBits == totalBits)
        return 0.0;
    double p = setBits / totalBits;
    return -p * log2(p) - (1 - p) * log2(1 - p);
}

uint32_t DDosDetect::countBit(uint8_t *msg, const uint32_t length)
{
    uint64_t v, set_bits = 0;
    const uint64_t *ptr = (uint64_t *)msg;
    const uint64_t *end = (uint64_t *)(msg + length);
    do
    {
        v = *(ptr++);
        v = v - ((v >> 1) & 0x5555555555555555);
        v = (v & 0x3333333333333333) + ((v >> 2) & 0x3333333333333333);
        v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0F;
        set_bits += (v * 0x0101010101010101) >> (sizeof(v) - 1) * 8;
    } while (end > ptr);

    return set_bits;
}

int DDosDetect::ddosDetect(struct rte_mbuf *pkt)
{
    static char flag = 0;
    uint8_t *msg = rte_pktmbuf_mtod(pkt, uint8_t *);
    uint32_t setBit = countBit(msg, pkt->pkt_len);
    uint32_t totalBit = pkt->pkt_len * 8;
    _p_setbits[_pktIdx % CAPTURE_WINDOWS] = setBit;
    _p_totbits[_pktIdx % CAPTURE_WINDOWS] = totalBit;
    _p_entropy[_pktIdx % CAPTURE_WINDOWS] = ddosEntropy(setBit, totalBit);

    if (_pktIdx >= CAPTURE_WINDOWS)
    {
        int i = 0;
        uint32_t totalSet, totalBits = 0;
        double sumEntropy = 0.0;
        for (i = 0; i < CAPTURE_WINDOWS; ++i)
        {
            totalSet += _p_setbits[i];
            totalBit += _p_totbits[i];
            sumEntropy += _p_entropy[i];
        }
        double avgEntropy = ddosEntropy(totalSet, totalBit);
        SPDLOG_INFO("Avg Entropy: %f, Sum Entropy: %f", avgEntropy, sumEntropy);
        _pktIdx = (++_pktIdx) % CAPTURE_WINDOWS + CAPTURE_WINDOWS;
    }
    else
    {
        ++_pktIdx;
    }
    return 0;
}