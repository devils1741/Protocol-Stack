#include "Arp.hpp"

std::list<ArpHeader> ArpTable::_list;
std::mutex ArpTable::_mutex;
        
bool ArpHeader::operator==(const ArpHeader &other) const
{
    if (hardware_type != other.hardware_type)
        return false;

    if (!equal(sender_hwaddr, sender_hwaddr + sizeof(sender_hwaddr), other.sender_hwaddr))
        return false;

    if (sender_protoaddr != other.sender_protoaddr)
        return false;
    return true;
}

ArpTable &ArpTable::getInstance()
{
    static ArpTable _arpTable;
    return _arpTable;
}

int ArpTable::pushBack(ArpHeader arpHeader)
{
    lock_guard<mutex> lock(_mutex);
    _list.push_back(arpHeader);
    return 0;
}

int ArpTable::remove(const ArpHeader &arpHeader)
{
    lock_guard<mutex> lock(_mutex);

    for (auto it = _list.begin(); it != _list.end();)
    {
        if (*it == arpHeader)
        {
            it = _list.erase(it);
            return 0;
        }
        ++it;
    }
    return -1;
}

int ArpTable::clear()
{
    lock_guard<mutex> lock(_mutex);
    _list.clear();
    return 0;
}

uint8_t *ArpTable::search(const uint32_t &dip)
{
    lock_guard<mutex> lock(_mutex);
    for (auto &it : _list)
    {
        if (it.sender_protoaddr == dip)
        {
            return it.sender_hwaddr;
        }
    }
    return nullptr;
}
