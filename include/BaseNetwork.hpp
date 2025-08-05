#ifndef BASE_NETWORK_HPP
#define BASE_NETWORK_HPP
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <list>
#include <memory>

struct LocalHost
{
    int fd;                               ///< 文件描述符
    uint32_t localIp;                     ///< 本地IP地址
    uint8_t localMac[RTE_ETHER_ADDR_LEN]; ///< 本地MAC地址
    uint8_t protocal;                     ///< 协议类型
    struct rte_ring *sndbuf;              ///< 发送缓冲区
    struct rte_ring *rcvbuf;              ///< 接收缓冲区
    pthread_cond_t cond;                  ///< 条件变量，用于线程间同步
    pthread_mutex_t mutex;                ///< 互斥锁，用于保护条件变量
};

class BaseNetwork
{
public:
    static BaseNetwork &getInstance()
    {
        static BaseNetwork _instance;
        return _instance;
    }
    int getFdFromBitMap();
    int freeFdFromBitMap(int &fd);
    void *getHostInfoFromFd(int fd);
    struct localhost *get_hostinfo_fromip_port(uint32_t dip, uint16_t port, uint8_t proto);
    int createSocket(__attribute__((unused)) int domain, int type, __attribute__((unused)) int protocol);

private:
    BaseNetwork() = default;                              ///< 私有构造函数，禁止外部实例化
    ~BaseNetwork() = default;                             ///< 私有析构函数，禁止外部删除
    BaseNetwork(const BaseNetwork &) = delete;            ///< 禁止拷贝构造
    BaseNetwork &operator=(const BaseNetwork &) = delete; ///< 禁止赋值操作
    BaseNetwork(BaseNetwork &&) = delete;                 ///< 禁止移动构造
    BaseNetwork &operator=(BaseNetwork &&) = delete;      ///< 禁止移动赋值

private:
    static std::list<LocalHost*> _hostList; ///< 本地主机信息
    static unsigned char _fdTable[1024];   ///< 文件描述符位图
};

#endif