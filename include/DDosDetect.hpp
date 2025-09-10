#ifndef DDOS_DETECT_HPP
#define DDOS_DETECT_HPP
#define CAPTURE_WINDOWS 256
#include <vector>
#include <cstdint>
#include <math.h>

class DDosDetect
{
public:
    double ddosEntropy(double setBits, double totalBits);
    uint32_t countBit(uint8_t *msg, const uint32_t length);
    int ddosDetect(struct rte_mbuf *pkt);

private:
    std::vector<uint32_t> _p_setbits{CAPTURE_WINDOWS};
    std::vector<uint32_t> _p_totbits{CAPTURE_WINDOWS};
    std::vector<double> _p_entropy{CAPTURE_WINDOWS};
    int _pktIdx = 0;
    double _tresh = 1200.0;
};

#endif