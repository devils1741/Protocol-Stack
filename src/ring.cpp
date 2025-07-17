#include "ring.hpp"

struct inout_ring *Ring::getInstance()
{
    if (_ring == nullptr)
    {
        _ring = static_cast<struct inout_ring *>(rte_malloc("in/out ring", sizeof(struct inout_ring), 0));
        memset(_ring, 0, sizeof(struct inout_ring));
    }
    return _ring;
}