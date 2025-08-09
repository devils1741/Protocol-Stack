#ifndef BASE_NETWORK_HPP
#define BASE_NETWORK_HPP
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <list>
#include <memory>

class BaseNetwork
{
public:
    BaseNetwork() = default; 
    ~BaseNetwork() = default;
    int allocFdFromBitMap();
    int searchFdFromBitMap(int &fd);
    int freeFdFromBitMap(int &fd);
    void *getHostInfoFromFd(int &fd);

private:
    BaseNetwork(const BaseNetwork &) = delete;            ///< 禁止拷贝构造
    BaseNetwork &operator=(const BaseNetwork &) = delete; ///< 禁止赋值操作
    BaseNetwork(BaseNetwork &&) = delete;                 ///< 禁止移动构造
    BaseNetwork &operator=(BaseNetwork &&) = delete;      ///< 禁止移动赋值

private:
    static unsigned char _fdTable[1024];        ///< 文件描述符位图
};

#endif