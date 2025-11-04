#ifndef ICMP__HPP
#define ICMP__HPP
#include <rte_memory.h>
#include "Processor.hpp"

class IcmpProcessor : public Processor
{
public:
    /**
     * @brief  获取全局唯一实例
     * @return 实例引用
     */
    static IcmpProcessor &getInstance()
    {
        static IcmpProcessor instance;
        return instance;
    }
    /**
     * @brief  处理一个入站报文（回调函数）
     *
     * 若 IP 上层协议为 ICMP 且类型为 Echo Request，
     * 则构造 Echo Reply 并发往 @p ring->out ，最后释放原报文。
     *
     * @param[in]     mbufPool  用于分配回复包的内存池
     * @param[in,out] mbuf      入站报文（函数内会 free）
     * @param[in]     ring      输出环，回复包将入队到 ring->out
     * @return 0 成功；<0 失败
     */
    int handlePacket(struct rte_mempool *mbufPool, struct rte_mbuf *mbuf, struct inout_ring *ring);

    /**
     * @brief  构造 ICMP Echo Reply 报文（mbuf 级别）
     *
     * @param[in] mbufPool  内存池
     * @param[in] dstMac    目标 MAC
     * @param[in] sip       源 IPv4
     * @param[in] dip       目的 IPv4
     * @param[in] id        ICMP Identifier
     * @param[in] seqNb     ICMP Sequence
     * @param[in] payload   指向 完整 ICMP 报文
     * @param[in] payload_len 报文长度（字节）
     * @return 构造好的 mbuf；失败返回 nullptr
     */
    struct rte_mbuf *sendIcmpPacket(struct rte_mempool *mbufPool, uint8_t *dstMac,
                                    uint32_t sip, uint32_t dip, uint16_t id, uint16_t seqNb, uint8_t *payload, uint16_t payload_len);

    /**
     * @brief  底层编码函数：填充 Ether + IPv4 + ICMP 头部
     *
     * @param[out] msg        已分配缓冲区，长度 ≥ ETH+IP+payload_len
     * @param[in]  dstMac     目标 MAC
     * @param[in]  srcIp      源 IPv4
     * @param[in]  dstIp      目的 IPv4
     * @param[in]  id         ICMP Identifier
     * @param[in]  seqNb      ICMP Sequence
     * @param[in]  payload    待拷贝的 ICMP 报文
     * @param[in]  payload_len 长度
     * @return 0 成功；-1 参数错误
     */
    int encodeIcmpPkt(uint8_t *msg, uint8_t *dstMac,
                      uint32_t srcIp, uint32_t dstIp, uint16_t id, uint16_t seqNb, uint8_t *payload, uint16_t payload_len);

    /**
     * @brief  设置下游 Processor（责任链模式，当前为空实现）
     * @return 0
     */
    int setNextProcessor(std::shared_ptr<Processor> nextProcessor);
    /**
     * @brief  计算 ICMP 校验和（RFC792）
     * @param[in] addr  起始地址，按 16-bit 对齐
     * @param[in] count 长度（字节），允许奇数
     * @return 16-bit 校验和，已取反
     */
    uint16_t ng_checksum(uint16_t *addr, int count);

private:
    IcmpProcessor() = default;
    ~IcmpProcessor() = default;
    IcmpProcessor(const IcmpProcessor &) = delete;
    IcmpProcessor &operator=(const IcmpProcessor &) = delete;
    IcmpProcessor(IcmpProcessor &&) = delete;
    IcmpProcessor &operator=(IcmpProcessor &&) = delete;

private:
    std::shared_ptr<Processor> _nextProcessor; ///< 下游处理器（预留）
};

#endif