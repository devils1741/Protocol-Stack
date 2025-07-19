#ifndef DPDK_MANAGER_HPP
#define DPDK_MANAGER_HPP

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <memory>
#include <string>
#include "Logger.hpp"

using std::string;

/**
 * @brief DPDKManager类用于管理DPDK相关资源,主要是一个DPDK内存池,还有一些参数例如内存池名称，内存池内部的mbuf数量和socket ID等
 */
class DPDKManager
{
public:
    /**
     * @brief 默认构造函数, 创建一个DPDKManager对象,同时初始化一个mbuf池
     * @param name 内存池名称
     * @param NUM_MBUFS 内存池中mbuf的数量
     * @param socket_id 绑定的socket ID
     * @return DPDKManager对象
     * @throws 如果mbuf池创建失败,则直接退出程序
     */
    DPDKManager(const string &name, unsigned NUM_MBUFS, int socket_id);

    /**
     * @brief 析构函数,释放mbuf池资源
     */
    ~DPDKManager();

    /**
     * @brief 获取mbuf池的const指针
     */
    rte_mempool *getMbufPool() const;

    /**
     * @brief 初始化DPDK端口
     * @param portID 端口ID
     * @param port_conf_default 端口配置
     * @return 成功时返回0,失败时直接退出程序
     */
    int initPort(int portID, rte_eth_conf port_conf_default);

private:
    DPDKManager(const DPDKManager &) = delete;            ///< 禁止拷贝构造函数
    DPDKManager(DPDKManager &&) = delete;                 ///< 禁止移动构造函数
    DPDKManager &operator=(const DPDKManager &) = delete; ///< 禁止拷贝赋值操作符
    DPDKManager &operator=(DPDKManager &&) = delete;      ///< 禁止移动赋值操作符

private:
    struct rte_mempool *_mbufPool = nullptr; ///< mbuf池指针
    string _name;                            ///< 内存池名称
    unsigned _NUM_MBUFS;                     ///< mbuf池中内存块的数量
    int _socket_id;                          ///< 绑定的socket ID
};

#endif
