#include "Arp.hpp"

list<ArpHeader> *ArpTable::_list = nullptr;
mutex ArpTable::_mutex;

bool ArpHeader::operator==(const ArpHeader &other) const
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

ArpTable &ArpTable::getInstance()
{
    static ArpTable _arpTable;
    if (_list == nullptr)
    {
        _list = new list<ArpHeader>();
    }
    return _arpTable;
}

int ArpTable::pushBack(ArpHeader arpHeader)
{
    lock_guard<mutex> lock(_mutex);
    _list->push_back(arpHeader);
    return 0;
}

int ArpTable::remove(const ArpHeader &arpHeader)
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

int ArpTable::clear()
{
    lock_guard<mutex> lock(_mutex);
    if (_list != nullptr)
    {
        _list->clear();
        return 0;
    }
    return -1;
}
