#ifndef DDOS_DETECT_HPP
#define DDOS_DETECT_HPP
#define CAPTURE_WINDOWS 256
#include <vector>
#include <cstdint>
#include <math.h>

class DDosDetect
{
public:
    static double ddosEntropy(double setBits, double totalBits);
    static uint32_t countBit(uint8_t *msg, const uint32_t length);
    static int ddosDetect(struct rte_mbuf *pkt);

private:
    uint32_t _p_setbits[CAPTURE_WINDOWS] = {0};
    uint32_t _p_totbits[CAPTURE_WINDOWS] = {0};
    double p_entropy[CAPTURE_WINDOWS] = {0.0};
    int _pktIddx = 0;
};

#endif