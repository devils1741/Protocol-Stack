#include <gtest/gtest.h>
#include "Arp.hpp"
#include <thread>

/**
 * @brief  ARP 测试的测试类
 */
class ArpTest : public ::testing::Test
{
protected:
    /**
     * @brief 在每个测试之前清空 ARP 表
     */
    void SetUp() override
    {
        ArpTable::getInstance().clear();
    }
};

/**
 * @brief 测试 ArpHeader 的相等性操作符
 */
TEST_F(ArpTest, ArpHeaderEquality)
{
    ArpHeader header1 = {
        .hardware_type = 1,
        .sender_hwaddr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05},
        .sender_protoaddr = 0xC0A80001, // 192.168.0.1
    };

    ArpHeader header2 = header1; // 完全相同的副本
    ArpHeader header3 = header1; // 创建副本以进行修改

    // 测试完全相同的头部
    EXPECT_TRUE(header1 == header2);

    // 测试不同的硬件类型
    header3.hardware_type = 2;
    EXPECT_FALSE(header1 == header3);

    // 重置并测试不同的发送方硬件地址
    header3 = header1;
    header3.sender_hwaddr[0] = 0xFF;
    EXPECT_FALSE(header1 == header3);

    // 重置并测试不同的发送方协议地址
    header3 = header1;
    header3.sender_protoaddr = 0xC0A80003;
    EXPECT_FALSE(header1 == header3);
}

/**
 * @brief 测试 ArpTable 的单例特性
 */
TEST_F(ArpTest, ArpTableSingleton)
{
    // 检查两次调用 getInstance 是否返回相同的实例
    EXPECT_EQ(&ArpTable::getInstance(), &ArpTable::getInstance());
}

/**
 * @brief 测试 ArpTable 的 pushBack 和 remove 操作
 */
TEST_F(ArpTest, ArpTablePushRemove)
{
    ArpHeader header = {
        .hardware_type = 1,
        .sender_hwaddr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05},
        .sender_protoaddr = 0xC0A80001,
    };

    // 测试 pushBack 操作
    EXPECT_EQ(ArpTable::getInstance().pushBack(header), 0);

    // 测试移除存在的条目
    EXPECT_EQ(ArpTable::getInstance().remove(header), 0);

    // 测试移除不存在的条目
    EXPECT_EQ(ArpTable::getInstance().remove(header), -1);
}

/**
 * @brief 测试 ArpTable 的线程安全性
 * 没有显式断言，因为主要测试是否会发生崩溃
 * 互斥锁应该可以防止数据竞争
 */
TEST_F(ArpTest, ArpTableThreadSafety)
{
    ArpHeader header = {
        .hardware_type = 1,
        .sender_hwaddr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05},
        .sender_protoaddr = 0xC0A80001,
    };

    // 创建多个线程并发执行 push 和 remove 操作
    auto push_task = [&header]()
    {
        for (int i = 0; i < 100; ++i)
        {
            ArpTable::getInstance().pushBack(header);
        }
    };

    auto remove_task = [&header]()
    {
        for (int i = 0; i < 100; ++i)
        {
            ArpTable::getInstance().remove(header);
        }
    };

    std::thread t1(push_task);
    std::thread t2(remove_task);

    t1.join();
    t2.join();
}

// 主函数，用于运行测试
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}