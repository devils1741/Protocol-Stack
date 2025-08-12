#include "BaseNetwork.hpp"
#include <arpa/inet.h>
#include <rte_malloc.h>
#include "ConfigManager.hpp"

#define DEFAULT_FD_NUM 3
#define MAX_FD_COUNT 1024
unsigned char BaseNetwork::_fdTable[1024] = {0};

int BaseNetwork::allocFdFromBitMap()
{
    int fd = DEFAULT_FD_NUM;
    for (; fd < MAX_FD_COUNT; ++fd)
    {
        if ((_fdTable[int(fd / 8)] & (0x1 << fd % 8)) == 0)
        {
            _fdTable[(int)(fd / 8)] |= (0x1 << fd % 8);
            return fd;
        }
    }
    return -1;
}

int BaseNetwork::searchFdFromBitMap(int fd)
{
    int result = _fdTable[int(fd / 8)] & (0x1 << fd % 8);
    return result;
}

int BaseNetwork::freeFdFromBitMap(int fd)
{
    if (fd < DEFAULT_FD_NUM || fd >= MAX_FD_COUNT)
        return -1;
    if ((_fdTable[int(fd / 8)] & (0x1 << fd % 8)) == 0)
        return -1;
    _fdTable[int(fd / 8)] &= ~(0x1 << fd % 8);
    return 0;
}
