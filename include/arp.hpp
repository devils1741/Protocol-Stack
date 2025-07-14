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
 * @brief arp的数据结构
 */
struct ArpHeader
{
    uint16_t hardware_type;    // 硬件类型
    uint16_t protocol_type;    // 协议类型
    uint8_t hardware_length;   // 硬件地址长度
    uint8_t protocol_length;   // 协议地址长度
    uint16_t operation;        // 操作码
    uint8_t sender_hwaddr[6];  // 发送方硬件地址
    uint32_t sender_protoaddr; // 发送方协议地址
    uint8_t target_hwaddr[6];  // 目标硬件地址
    uint32_t target_protoaddr; // 目标协议地址

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

class ArpTable
{
public:
    static ArpTable &GetInstance()
    {
        static ArpTable _arpTable;
        if (_list == nullptr)
        {
            _list = new list<ArpHeader>();
        }
        return _arpTable;
    }

    static int push_back(ArpHeader arpHeader)
    {
        lock_guard<mutex> lock(_mutex);
        _list->push_back(arpHeader);
        return 0;
    }

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
    static list<ArpHeader> *_list;
    static mutex _mutex;
};

list<ArpHeader> *ArpTable::_list = nullptr;
mutex ArpTable::_mutex;

#endif
