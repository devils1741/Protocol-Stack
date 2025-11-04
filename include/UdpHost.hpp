#ifndef UDP_HOST_HPP
#define UDP_HOST_HPP
#include <cstdint>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <list>
#include <mutex>
#include "BaseNetwork.hpp"

struct UdpHost
{
    int fd;                               ///< 文件描述符
    uint32_t localIp;                     ///< 本地IP地址
    uint8_t localMac[RTE_ETHER_ADDR_LEN]; ///< 本地MAC地址
    uint16_t localport;                   ///< 本地端口
    uint8_t protocal;                     ///< 协议类型
    struct rte_ring *sndbuf;              ///< 发送缓冲区
    struct rte_ring *rcvbuf;              ///< 接收缓冲区
    pthread_cond_t cond;                  ///< 条件变量，用于线程间同步
    pthread_mutex_t mutex;                ///< 互斥锁，用于保护条件变量
};

struct offload
{
    uint32_t sip;        ///< 源IP
    uint32_t dip;        ///< 目的IP
    uint16_t sport;      ///< 源端口好
    uint16_t dport;      ///< 目的端口号
    int protocol;        ///< 协议类型(
    unsigned char *data; ///< 数据区
    uint16_t length;     ///< 数据包长度
};

class UdpServerManager : public BaseNetwork
{
public:
    /**
     * @brief 获取单例
     */
    static UdpServerManager &getInstance()
    {
        static UdpServerManager instance;
        return instance;
    }
    /**
     * @brief  根据本地 IP+端口+协议 查找 UdpHost
     * @param  dip   本地 IPv4（网络字节序）
     * @param  port  本地端口（网络字节序）
     * @param  proto 协议号，如 IPPROTO_UDP
     * @return 指针；未找到返回 nullptr
     */
    struct UdpHost *getHostInfoFromIpAndPort(uint32_t dip, uint16_t port, uint8_t proto);
    /**
     * @brief  创建 UDP socket
     * @param  domain   协议族，仅支持 AF_INET
     * @param  type     套接字类型，仅支持 SOCK_DGRAM
     * @param  protocol 传输层协议号，当前忽略（固定为 IPPROTO_UDP）
     * @return 成功返回 fd（≥ 0）；失败返回 -1
     * @note   内部会分配 UdpHost、sndbuf/rcvbuf 环形队列及 pthread 同步对象
     */
    int nsocket(__attribute__((unused)) int domain, int type, __attribute__((unused)) int protocol);

    /**
     * @brief  绑定本地地址
     * @param  sockfd 由 nsocket 返回的 fd
     * @param  addr   本地 IPv4+端口（sockaddr_in）
     * @param  addrlen 地址长度，当前忽略
     * @return 0 成功；-1 失败（fd 不存在或已绑定）
     */
    int nbind(int sockfd, const struct sockaddr *addr, __attribute__((unused)) socklen_t addrlen);

    /**
     * @brief  接收数据报文
     * @param  sockfd   本地 socket
     * @param  buf      用户缓冲区
     * @param  len      缓冲区长度
     * @param  flags    标志位，当前忽略
     * @param  src_addr 输出：对端地址（sockaddr_in）
     * @param  addrlen  输入/输出：地址长度，当前忽略
     * @return 实际拷贝字节数；<0 表示错误
     * @note   若用户缓冲区小于数据报，则剩余部分重新入队，下次返回
     */
    ssize_t nrecvfrom(int sockfd, void *buf, size_t len, __attribute__((unused)) int flags, struct sockaddr *src_addr, __attribute__((unused)) socklen_t *addrlen);
    /**
     * @brief  发送数据报
     * @param  sockfd   本地 socket
     * @param  buf      待发送数据
     * @param  len      数据长度
     * @param  flags    标志位，当前忽略
     * @param  dest_addr 对端地址（sockaddr_in）
     * @param  addrlen   地址长度，当前忽略
     * @return 实际入队字节数；<0 表示错误
     * @note   仅将 offload 放入 sndbuf
     */
    ssize_t nsendto(int sockfd, const void *buf, size_t len, __attribute__((unused)) int flags, const struct sockaddr *dest_addr, __attribute__((unused)) socklen_t addrlen);

    /**
     * @brief  关闭 socket 并释放资源
     * @param  fd 要关闭的 fd
     * @return 0 成功；-1 失败（fd 不存在）
     * @note   会自动从 _udpHostList 移除、释放 rte_ring 与 UdpHost 内存
     */
    int nclose(int fd);

    /**
     * @brief  演示用 UDP 回显服务器线程函数
     * @param  arg 未使用
     * @return 0
     * @note   内部循环：recvfrom → 打印 → sendto
     */
    int udpServer(__attribute__((unused)) void *arg);
    /**
     * @brief  获取整个 UDP 主机列表（调试用）
     * @return 引用，外部只读访问；写操作需自行加锁
     */
    std::list<UdpHost *> &getUdpHostList();
    /**
     * @brief  根据 fd 查找 UdpHost
     * @param  fd socket 描述符
     * @return 指针；未找到返回 nullptr
     */
    UdpHost *getHostInfoFromFd(int fd);
    /**
     * @brief  从管理列表移除并释放指定 host
     * @param  host 必须是由本管理器创建的指针
     * @return 0 成功；-1 失败（未找到）
     */
    int removeHost(UdpHost *host);

private:
    UdpServerManager() = default;
    ~UdpServerManager() = default;
    UdpServerManager(const UdpServerManager &) = delete;
    UdpServerManager &operator=(const UdpServerManager &) = delete;
    UdpServerManager(UdpServerManager &&) = delete;
    UdpServerManager &operator=(UdpServerManager &&) = delete;

private:
    std::list<UdpHost *> _udpHostList; ///< 所有存活 UdpHost
    std::mutex _mutex;                 ///< 保护 _udpHostList
};

#endif