#include <iostream>
#include <fstream>
#include "Json.hpp"
#include <mutex>
#include <arpa/inet.h>

// 使用 nlohmann/json 的命名空间
using json = nlohmann::json;

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
        _ring_size = _json["RING_SIZE"].get<int>();
        _timer_resolution_cycles = _json["TIMER_RESOLUTION_CYCLES"].get<unsigned long long>();
        _local_ip = _json["LOCAL_IP"].get<std::string>();
        _local_addr = inet_addr(_local_ip.c_str());
        _gDpdkPortId = _json["gDpdkPortId"].get<int>();
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
    uint32_t getRingSize() const { return _ring_size; }
    string getLocalIp() const { return _local_ip; }
    uint32_t getLocalAddr() const { return _local_addr; }
    int getDpdkPortId() const { return _gDpdkPortId; }
    unsigned long long getTimerResolutionCycles() const { return _timer_resolution_cycles; }

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
    unsigned long long _timer_resolution_cycles = 0;
    std::string _local_ip;
    uint32_t _local_addr = 0;
    int _gDpdkPortId = 0;
};
