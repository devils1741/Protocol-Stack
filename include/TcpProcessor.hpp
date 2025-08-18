#ifndef TCP_PROCESSOR_HPP
#define TCP_PROCESSOR_HPP
#include "TcpHost.hpp"

class TcpProcessor
{
public:
    static TcpProcessor &getInstance()
    {
        static TcpProcessor instance;
        return instance;
    }
    int tcpProcess(struct rte_mbuf *tcpmbuf);
    int tcpHandleListen(struct TcpStream *listenStream, struct rte_tcp_hdr *tcphdr, struct rte_ipv4_hdr *iphdr);
    struct TcpStream *tcpCreateStream(uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort);
    int tcpHandleSynRcvd(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr);
    int tcpHandleEstablished(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr, int tcplen);
    int tcpEnqueueRecvbuffer(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr, int tcplen);
    int tcpSendAckpkt(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr);
    int tcpHandleCloseWait(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr);
    int tcpHandleLastAck(struct TcpStream *ts, struct rte_tcp_hdr *tcphdr);
    int tcpOut(struct rte_mempool *mbufPool);
    struct rte_mbuf *TcpPkt(struct rte_mempool *mbuf_pool, uint32_t sip, uint32_t dip,
                            uint8_t *srcmac, uint8_t *dstmac, struct TcpFragment *fragment);
    int encodeTcpApppkt(uint8_t *msg, uint32_t sip, uint32_t dip,
                        uint8_t *srcmac, uint8_t *dstmac, struct TcpFragment *fragment);

private:
    TcpProcessor() = default;
    ~TcpProcessor() = default;
    TcpProcessor(const TcpProcessor &) = delete;
    TcpProcessor &operator=(const TcpProcessor &) = delete;
    TcpProcessor(TcpProcessor &&) = delete;
    TcpProcessor &operator=(TcpProcessor &&) = delete;
};

#endif