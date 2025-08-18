#include <iostream>
#include <fstream>
#include "Json.hpp"
#include <mutex>
#include <arpa/inet.h>

// 使用 nlohmann/json 的命名空间
using json = nlohmann::json;
#define MAKE_IPV4_ADDR(a, b, c, d) (a + (b << 8) + (c << 16) + (d << 24))
/**
 * @brief ConfigManager 类用于读取JSON配置文件中的参数
 */
class ConfigManager
{
public:
    // 获取单例实例
    static ConfigManager &getInstance()
    {
        static ConfigManager _instance; // 局部静态变量，保证线程安全
        return _instance;
    }

    // 从文件加载配置
    bool loadConfig(const std::string &filename)
    {
        // 打开 JSON 文件
        std::ifstream file(filename);
        _json = json::parse(file);

        // 读取配置
        _num_mbufs = _json["NUM_MBUFS"].get<int>();
        _burst_size = _json["BURST_SIZE"].get<int>();
        _buffer_size = _json["BUFFER_SIZE"].get<int>();
        _ring_size = _json["RING_SIZE"].get<int>();
        _timer_resolution_cycles = _json["TIMER_RESOLUTION_CYCLES"].get<unsigned long long>();
        _local_ip = _json["LOCAL_IP"].get<std::string>();
        struct in_addr addr;
        inet_pton(AF_INET, _local_ip.c_str(), &addr); // addr.s_addr 现在是网络字节序
        _local_addr = MAKE_IPV4_ADDR(192, 168, 0, 104);
        _dpdk_port_id = _json["DPDK_PORT_ID"].get<int>();
        return true;
    }

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "\n"
            << "LOCAL_IP: " << _local_ip << "\n"
            << "LOCAL_ADDR: " << _local_addr << "\n"
            << "NUM_MBUFS: " << _num_mbufs << "\n"
            << "BURST_SIZE: " << _burst_size << "\n"
            << "RING_SIZE: " << _ring_size << "\n"
            << "TIMER_RESOLUTION_CYCLES: " << _timer_resolution_cycles;

        return oss.str();
    }

    // 获取配置值
    uint32_t getNumMbufs() const { return _num_mbufs; }
    uint32_t getBurstSize() const { return _burst_size; }
    uint32_t getBufferSize() const { return _buffer_size; }
    uint32_t getRingSize() const { return _ring_size; }
    std::string getLocalIp() const { return _local_ip; }
    uint32_t getLocalAddr() const { return _local_addr; }
    int getDpdkPortId() const { return _dpdk_port_id; }
    unsigned long long getTimerResolutionCycles() const { return _timer_resolution_cycles; }
    uint8_t *getSrcMac() { return _src_mac; }

private:
    // 私有构造函数
    ConfigManager() = default;
    // 禁止拷贝构造和赋值操作
    ConfigManager(const ConfigManager &) = delete;
    ConfigManager(ConfigManager &&) = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;
    ConfigManager &operator=(ConfigManager &&) = delete;

    json _json; // 用于存储 JSON 数据
    uint32_t _num_mbufs = 0;
    uint32_t _burst_size = 0;
    uint32_t _ring_size = 0;
    uint32_t _buffer_size = 0;
    unsigned long long _timer_resolution_cycles = 0;
    std::string _local_ip;
    uint32_t _local_addr = 0;
    int _dpdk_port_id = 0;
    uint8_t _src_mac[RTE_ETHER_ADDR_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
};
