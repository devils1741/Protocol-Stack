#ifndef __ARP_H__
#define __ARP_H__

#include <rte_ether.h>
#include <list>
#include <algorithm>
#include <mutex>

using std::equal;
using std::list;
using std::lock_guard;
using std::mutex;
/**
 * @brief arp的数据结构,根据协议RFC826定义,协议文档地址:https://www.rfc-editor.org/rfc/pdfrfc/rfc826.txt.pdf
 */
struct ArpHeader
{
    uint16_t hardware_type = 0xFFFF;        ///< 硬件类型
    uint16_t protocol_type = 0xFFFF;        ///< 协议类型
    uint8_t hardware_length = 0xFF;         ///< 硬件地址长度
    uint8_t protocol_length = 0xFF;         ///< 协议地址长度
    uint16_t operation = 0xFFFF;            ///< 操作码
    uint8_t sender_hwaddr[6] = {0xFF};      ///< 发送方硬件地址
    uint32_t sender_protoaddr = 0xFFFFFFFF; ///< 发送方协议地址
    uint8_t target_hwaddr[6] = {0xFF};      ///< 目标硬件地址
    uint32_t target_protoaddr = 0xFFFFFFFF; ///< 目标协议地址

    /**
     * @brief 比较两个ArpHeader是否相等，通过属性进行比较，全部相等才认为等价的
     * @return 相等返回true,否则返回false
     */
    bool operator==(const ArpHeader &other) const
    {
        if (hardware_type != other.hardware_type)
            return false;

        if (protocol_type != other.protocol_type)
            return false;

        if (hardware_length != other.hardware_length)
            return false;

        if (protocol_length != other.protocol_length)
            return false;

        if (operation != other.operation)
            return false;

        if (!equal(sender_hwaddr, sender_hwaddr + sizeof(sender_hwaddr), other.sender_hwaddr))
            return false;

        if (sender_protoaddr != other.sender_protoaddr)
            return false;

        if (!equal(target_hwaddr, target_hwaddr + sizeof(target_hwaddr), other.target_hwaddr))
            return false;

        if (target_protoaddr != other.target_protoaddr)
            return false;
        return true;
    }
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
    static ArpTable &getInstance()
    {
        static ArpTable _arpTable;
        if (_list == nullptr)
        {
            _list = new list<ArpHeader>();
        }
        return _arpTable;
    }

    /**
     * @brief 插入arp数据到队列尾部
     * @return 成功时返回0
     */
    static int pushBack(ArpHeader arpHeader)
    {
        lock_guard<mutex> lock(_mutex);
        _list->push_back(arpHeader);
        return 0;
    }

    /**
     * @brief 移除某个arp数据
     * @return 成功时返回0,存储队列为空或者找不到要移除的数据则返回-1
     */
    static int remove(const ArpHeader &arpHeader)
    {
        lock_guard<mutex> lock(_mutex);
        if (_list == nullptr)
        {
            return -1;
        }

        for (auto it = _list->begin(); it != _list->end();)
        {
            if (*it == arpHeader)
            {
                it = _list->erase(it);
                return 0;
            }
            ++it;
        }
        return -1;
    }

    /**
     * @brief 清空队列中的所有数据
     * @return 成功时返回0, 如果列表本来就是空则返回-1
     */
    static int clear()
    {
        lock_guard<mutex> lock(_mutex);
        if (_list != nullptr)
        {
            _list->clear();
            return 0;
        }
        return -1;
    }

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

list<ArpHeader> *ArpTable::_list = nullptr;
mutex ArpTable::_mutex;

#endif
