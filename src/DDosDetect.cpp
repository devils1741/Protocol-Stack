#include "DDosDetect.hpp"

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