#ifndef __ARP_H__
#define __ARP_H__
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <list>
#include <algorithm>
#include <mutex>
#include <arpa/inet.h>
#include "Logger.hpp"

using std::equal;
using std::list;
using std::lock_guard;
using std::mutex;
/**
 * @brief arp的数据结构,根据协议RFC826定义,协议文档地址:https://www.rfc-editor.org/rfc/pdfrfc/rfc826.txt.pdf
 */
struct ArpHeader
{
    uint16_t hardware_type = 0xFFFF;                    ///< 硬件类型
    uint16_t protocol_type = 0xFFFF;                    ///< 协议类型
    uint8_t hardware_length = 0xFF;                     ///< 硬件地址长度
    uint8_t protocol_length = 0xFF;                     ///< 协议地址长度
    uint16_t operation = 0xFFFF;                        ///< 操作码
    uint8_t sender_hwaddr[RTE_ETHER_ADDR_LEN] = {0xFF}; ///< 发送方硬件地址
    uint32_t sender_protoaddr = 0xFFFFFFFF;             ///< 发送方协议地址
    uint8_t target_hwaddr[RTE_ETHER_ADDR_LEN] = {0xFF}; ///< 目标硬件地址
    uint32_t target_protoaddr = 0xFFFFFFFF;             ///< 目标协议地址

    /**
     * @brief 比较两个ArpHeader是否相等，通过属性进行比较，全部相等才认为等价的
     * @return 相等返回true,否则返回false
     */
    bool operator==(const ArpHeader &other) const;
};

/**
 * @brief 存储arp地址的类,单例模式，有插入、查找、移除的功能
 */

class ArpTable
{
public:
    /**
     * @brief 获取唯一对象
     * @return 静态类本身
     */
    static ArpTable &getInstance();
    /**
     * @brief 插入arp数据到队列尾部
     * @return 成功时返回0
     */
    static int pushBack(ArpHeader arpHeader);

    /**
     * @brief 移除某个arp数据
     * @return 成功时返回0,存储队列为空或者找不到要移除的数据则返回-1
     */
    static int remove(const ArpHeader &arpHeader);

    /**
     * @brief 清空队列中的所有数据
     * @return 成功时返回0, 如果列表本来就是空则返回-1
     */
    static int clear();

private:
    ArpTable() {};
    ~ArpTable() {};
    ArpTable(const ArpTable &) = delete;
    ArpTable(ArpTable &&) = delete;
    ArpTable &operator=(const ArpTable &) = delete;
    ArpTable &operator=(ArpTable &&) = delete;

private:
    static list<ArpHeader> *_list; ///< 存储arp数据
    static mutex _mutex;           ///< 互斥锁,用于实现线程安全
};

#endif
