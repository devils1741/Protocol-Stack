#ifndef __ARP_H__
#define __ARP_H__

#include <rte_ether.h>
#include <list>

using std::list;

/**
 * @brief arp的数据结构
 */
struct arp_header
{
    uint16_t hardware_type;      // 硬件类型
    uint16_t protocol_type;      // 协议类型
    uint8_t hardware_length;     // 硬件地址长度
    uint8_t protocol_length;     // 协议地址长度
    uint16_t operation;          // 操作码
    uint8_t sender_hwaddr[6];    // 发送方硬件地址
    uint8_t sender_protoaddr[4]; // 发送方协议地址
    uint8_t target_hwaddr[6];    // 目标硬件地址
    uint8_t target_protoaddr[4]; // 目标协议地址
};

#endif
