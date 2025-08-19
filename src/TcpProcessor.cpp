#include "TcpProcessor.hpp"
#include "TcpHost.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "ConfigManager.hpp"
#include "Arp.hpp"
#include "ArpProcessor.hpp"
#include <rte_malloc.h>

#define TCP_INITIAL_WINDOW 14600
#define TCP_MAX_SEQ 4294967295

int TcpProcessor::tcpProcess(struct rte_mbuf *tcpmbuf)
{
    struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(tcpmbuf, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
    struct rte_tcp_hdr *tcphdr = (struct rte_tcp_hdr *)(iphdr + 1);

    // tcphdr, rte_ipv4_udptcp_cksum
    uint16_t tcpcksum = tcphdr->cksum;
    tcphdr->cksum = 0;
    uint16_t cksum = rte_ipv4_udptcp_cksum(iphdr, tcphdr);

    if (cksum != tcpcksum)
    {
        SPDLOG_ERROR("cksum:{}, tcp cksum: {}", cksum, tcpcksum);
        return -1;
    }

    struct TcpStream *ts = TcpTable::getInstance().getTcpStream(iphdr->src_addr, iphdr->dst_addr, tcphdr->src_port, tcphdr->dst_port);
    if (ts == nullptr)
    {
        SPDLOG_ERROR("Create TcpStream failed");
        return -2;
    }

    switch (ts->status)
    {
    case TCP_STATUS::TCP_STATUS_CLOSED: // client
        break;

    case TCP_STATUS::TCP_STATUS_LISTEN: // server
    {
        int code = tcpHandleListen(ts, tcphdr, iphdr);
        if (code != 0)
        {
            SPDLOG_ERROR("TCP Listen failed. code={}", code);
        }
        break;
    }
    case TCP_STATUS::TCP_STATUS_SYN_RCVD: // server
        tcpHandleSynRcvd(ts, tcphdr);
        break;

    case TCP_STATUS::TCP_STATUS_SYN_SENT: // client
        break;

    case TCP_STATUS::TCP_STATUS_ESTABLISHED:
    { // server | client
        int tcplen = ntohs(iphdr->total_length) - sizeof(struct rte_ipv4_hdr);
        tcpHandleEstablished(ts, tcphdr, tcplen);
        break;
    }
    case TCP_STATUS::TCP_STATUS_FIN_WAIT_1: //  ~client
        break;

    case TCP_STATUS::TCP_STATUS_FIN_WAIT_2: // ~client
        break;

    case TCP_STATUS::TCP_STATUS_CLOSING: // ~client
        break;

    case TCP_STATUS::TCP_STATUS_TIME_WAIT: // ~client
        break;

    case TCP_STATUS::TCP_STATUS_CLOSE_WAIT: // ~server
        tcpHandleCloseWait(ts, tcphdr);
        break;

    case TCP_STATUS::TCP_STATUS_LAST_ACK: // ~server
        tcpHandleLastAck(ts, tcphdr);
        break;
    }

    return 0;
}

int TcpProcessor::tcpHandleListen(struct TcpStream *listenStream, struct rte_tcp_hdr *tcphdr, struct rte_ipv4_hdr *iphdr)
{
    SPDLOG_INFO("TCP Listen .... ");
    if (tcphdr->tcp_flags & RTE_TCP_SYN_FLAG)
    {
        if (listenStream->status == TCP_STATUS::TCP_STATUS_LISTEN)
        {
            struct TcpStream *ts = TcpProcessor::tcpCreateStream(iphdr->src_addr, iphdr->dst_addr, tcphdr->src_port, tcphdr->dst_port);
            if (ts == nullptr)
            {
                SPDLOG_ERROR("Create TcpFragment failed");
                return -1;
            }
            TcpTable::getInstance().addTcpStream(ts);

            struct TcpFragment *tf = static_cast<struct TcpFragment *>(rte_malloc("TcpFragment", sizeof(struct TcpFragment), 0));
            if (tf == nullptr)
            {
                SPDLOG_ERROR("Create TcpFragment failed");
                return -1;
            }
            memset(tf, 0, sizeof(struct TcpFragment));

            tf->srcPort = tcphdr->dst_port;
            tf->dstPort = tcphdr->src_port;

            SPDLOG_INFO("TCP listening src: {}:{} dst: {}:{}", convert_uint32_to_ip(ts->srcIp), ntohs(tcphdr->src_port),
                        convert_uint32_to_ip(ts->dstIp), ntohs(tcphdr->dst_port));
            tf->seqnum = ts->sndNxt;
            tf->acknum = ntohl(tcphdr->sent_seq) + 1;
            ts->rcvNxt = tf->acknum;

            tf->tcp_flags = (RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG);
            tf->windows = TCP_INITIAL_WINDOW;
            tf->hdrlen_off = 0x50;

            tf->data = nullptr;
            tf->length = 0;

            rte_ring_mp_enqueue(ts->sndbuf, tf);
            ts->status = TCP_STATUS::TCP_STATUS_SYN_RCVD;
        }
    }

    return 0;
}

struct TcpStream *TcpProcessor::tcpCreateStream(uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort)
{ 
    SPDLOG_INFO("Create TCP Stream ....");
    struct TcpStream *ts = static_cast<struct TcpStream *>(rte_malloc("TcpStream", sizeof(struct TcpStream), 0));
    if (ts == nullptr)
        return nullptr;

    ts->srcIp = srcIp;
    ts->dstIp = dstIp;
    ts->srcPort = srcPort;
    ts->dstPort = dstPort;
    ts->protocol = IPPROTO_TCP;
    ts->fd = -1;
    ts->status = TCP_STATUS::TCP_STATUS_LISTEN;

    SPDLOG_INFO("TcpStream create srcIp={}, dstIp={}, srcPort={}, dstPort={}", convert_uint32_to_ip(srcIp), convert_uint32_to_ip(dstIp), ntohs(srcPort), ntohs(dstPort));

    int RING_SIZE = ConfigManager::getInstance().getRingSize();
    ts->sndbuf = rte_ring_create("sndbuf", RING_SIZE, rte_socket_id(), 0);
    ts->rcvbuf = rte_ring_create("rcvbuf", RING_SIZE, rte_socket_id(), 0);

    uint32_t next_seed = time(nullptr);
    ts->sndNxt = rand_r(&next_seed) % TCP_MAX_SEQ;
    rte_memcpy(ts->localMac, ConfigManager::getInstance().getSrcMac(), RTE_ETHER_ADDR_LEN);

    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    rte_memcpy(&ts->cond, &blank_cond, sizeof(pthread_cond_t));

    pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
    rte_memcpy(&ts->mutex, &blank_mutex, sizeof(pthread_mutex_t));

    return ts;
}

int TcpProcessor::tcpHandleSynRcvd(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr)
{
    SPDLOG_INFO("Handle TCP SynRcvd ...");
    if (tcphdr->tcp_flags & RTE_TCP_ACK_FLAG)
    {
        if (stream->status == TCP_STATUS::TCP_STATUS_SYN_RCVD)
        {
            uint32_t acknum = ntohl(tcphdr->recv_ack);
            if (acknum == stream->sndNxt + 1)
            {
                SPDLOG_INFO("TCP connection established for stream {}", stream->fd);
            }
            stream->status = TCP_STATUS::TCP_STATUS_ESTABLISHED;
            // accept
            struct TcpStream *listenStream = TcpTable::getInstance().getTcpStream(0, 0, 0, stream->dstPort);
            if (listenStream == nullptr)
            {
                SPDLOG_ERROR("Listen stream not found for port {}", stream->dstPort);
                rte_exit(EXIT_FAILURE, "ng_tcp_stream_search failed\n");
            }
            pthread_mutex_lock(&listenStream->mutex);
            pthread_cond_signal(&listenStream->cond);
            pthread_mutex_unlock(&listenStream->mutex);
        }
    }
    return 0;
}

int TcpProcessor::tcpHandleEstablished(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr, int tcplen)
{
    SPDLOG_INFO("TCP Handle Established ...");
    if (tcphdr->tcp_flags & RTE_TCP_SYN_FLAG)
    {
        SPDLOG_INFO("TCP SYN flag received, but already established for stream {}", stream->fd);
        return -1;
    }
    if (tcphdr->tcp_flags & RTE_TCP_PSH_FLAG)
    {
        tcpEnqueueRecvbuffer(stream, tcphdr, tcplen);

        uint8_t hdrlen = tcphdr->data_off >> 4;
        int payloadlen = tcplen - hdrlen * 4;

        stream->rcvNxt = stream->rcvNxt + payloadlen;
        stream->sndNxt = ntohl(tcphdr->recv_ack);

        tcpSendAckpkt(stream, tcphdr);
    }
    if (tcphdr->tcp_flags & RTE_TCP_ACK_FLAG)
    {
        SPDLOG_INFO("TCP ACK flag received for stream {}", stream->fd);
    }
    if (tcphdr->tcp_flags & RTE_TCP_FIN_FLAG)
    {
        stream->status = TCP_STATUS::TCP_STATUS_CLOSE_WAIT;
        tcpEnqueueRecvbuffer(stream, tcphdr, tcphdr->data_off >> 4);
        stream->rcvNxt = stream->rcvNxt + 1;
        stream->sndNxt = ntohl(tcphdr->recv_ack);
        tcpSendAckpkt(stream, tcphdr);
    }

    return 0;
}
int TcpProcessor::tcpEnqueueRecvbuffer(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr, int tcplen)
{
    struct TcpFragment *rfragment = (struct TcpFragment *)rte_malloc("TcpFragment", sizeof(struct TcpFragment), 0);
    if (rfragment == nullptr)
        return -1;
    memset(rfragment, 0, sizeof(struct TcpFragment));

    rfragment->dstPort = ntohs(tcphdr->dst_port);
    rfragment->srcPort = ntohs(tcphdr->src_port);

    uint8_t hdrlen = tcphdr->data_off >> 4;
    int payloadlen = tcplen - hdrlen * 4;
    if (payloadlen > 0)
    {
        uint8_t *payload = (uint8_t *)tcphdr + hdrlen * 4;
        rfragment->data = (unsigned char *)rte_malloc("unsigned char *", payloadlen + 1, 0);
        if (rfragment->data == nullptr)
        {
            rte_free(rfragment);
            return -1;
        }
        memset(rfragment->data, 0, payloadlen + 1);
        rte_memcpy(rfragment->data, payload, payloadlen);
        rfragment->length = payloadlen;
    }
    else if (payloadlen == 0)
    {
        rfragment->length = 0;
        rfragment->data = nullptr;
    }
    rte_ring_mp_enqueue(stream->rcvbuf, rfragment);
    pthread_mutex_lock(&stream->mutex);
    pthread_cond_signal(&stream->cond);
    pthread_mutex_unlock(&stream->mutex);
    return 0;
}

int TcpProcessor::tcpSendAckpkt(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr)
{

    struct TcpFragment *ackfrag = (struct TcpFragment *)rte_malloc("TcpFragment", sizeof(struct TcpFragment), 0);
    if (ackfrag == nullptr)
        return -1;
    memset(ackfrag, 0, sizeof(struct TcpFragment));

    ackfrag->dstPort = tcphdr->src_port;
    ackfrag->srcPort = tcphdr->dst_port;
    SPDLOG_INFO("Sending ACK packet for stream {}: acknum={}, seqnum={}", stream->fd, stream->rcvNxt, ntohs(tcphdr->sent_seq));
    ackfrag->acknum = stream->rcvNxt;
    ackfrag->seqnum = stream->sndNxt;

    ackfrag->tcp_flags = RTE_TCP_ACK_FLAG;
    ackfrag->windows = TCP_INITIAL_WINDOW;
    ackfrag->hdrlen_off = 0x50;
    ackfrag->data = nullptr;
    ackfrag->length = 0;

    rte_ring_mp_enqueue(stream->sndbuf, ackfrag);

    return 0;
}

int TcpProcessor::tcpHandleCloseWait(struct TcpStream *stream, struct rte_tcp_hdr *tcphdr)
{

    if (tcphdr->tcp_flags & RTE_TCP_FIN_FLAG)
    { //
        if (stream->status == TCP_STATUS::TCP_STATUS_CLOSE_WAIT)
        {
            SPDLOG_INFO("TCP connection closing for stream {}", stream->fd);
        }
    }
    return 0;
}

int TcpProcessor::tcpHandleLastAck(struct TcpStream *ts, struct rte_tcp_hdr *tcphdr)
{
    if (tcphdr->tcp_flags & RTE_TCP_ACK_FLAG)
    {
        if (ts->status == TCP_STATUS::TCP_STATUS_LAST_ACK)
        {
            ts->status = TCP_STATUS::TCP_STATUS_CLOSED;
            SPDLOG_INFO("TCP connection closed for stream {}", ts->fd);
            TcpTable::getInstance().removeStream(ts);
            rte_ring_free(ts->sndbuf);
            rte_ring_free(ts->rcvbuf);
            rte_free(ts);
        }
    }
    return 0;
}

int TcpProcessor::tcpOut(struct rte_mempool *mbufPool)
{
    std::list<TcpStream *> tmplist = TcpTable::getInstance().getTcpStreamList();
    struct inout_ring *ring = Ring::getSingleton().getRing();
    for (auto &stream : tmplist)
    {
        if (stream->sndbuf == nullptr)
            continue; // listener

        struct TcpFragment *fragment = nullptr;
        int nb_snd = rte_ring_mc_dequeue(stream->sndbuf, (void **)&fragment);
        if (nb_snd < 0)
            continue;

        uint8_t *dstMac = ArpTable::getInstance().search(stream->srcIp);
        if (dstMac == nullptr)
        {
            uint8_t *dstMac = new uint8_t[RTE_ETHER_ADDR_LEN];
            struct rte_mbuf *arpbuf = ArpProcessor::getInstance().sendArpPacket(mbufPool, RTE_ARP_OP_REQUEST, ConfigManager::getInstance().getSrcMac(), stream->srcIp, dstMac, stream->dstIp);
            rte_ring_mp_enqueue_burst(ring->out, (void **)&arpbuf, 1, nullptr);
            rte_ring_mp_enqueue(stream->sndbuf, fragment);
        }
        else
        {
            struct rte_mbuf *tcpbuf = TcpPkt(mbufPool, stream->dstIp, stream->srcIp, stream->localMac, dstMac, fragment);
            rte_ring_mp_enqueue_burst(ring->out, (void **)&tcpbuf, 1, nullptr);

            if (fragment->data != nullptr)
                rte_free(fragment->data);
            rte_free(fragment);
        }
    }

    return 0;
}

struct rte_mbuf *TcpProcessor::TcpPkt(struct rte_mempool *mbuf_pool, uint32_t sip, uint32_t dip,
                                      uint8_t *srcmac, uint8_t *dstmac, struct TcpFragment *fragment)
{
    const unsigned total_len = fragment->length + sizeof(struct rte_ether_hdr) +
                               sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) +
                               fragment->optlen * sizeof(uint32_t);
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!mbuf)
    {
        rte_exit(EXIT_FAILURE, "ng_tcp_pkt rte_pktmbuf_alloc\n");
    }
    mbuf->pkt_len = total_len;
    mbuf->data_len = total_len;

    uint8_t *pktdata = rte_pktmbuf_mtod(mbuf, uint8_t *);

    encodeTcpApppkt(pktdata, sip, dip, srcmac, dstmac, fragment);

    return mbuf;
}

int TcpProcessor::encodeTcpApppkt(uint8_t *msg, uint32_t sip, uint32_t dip,
                                  uint8_t *srcmac, uint8_t *dstmac, struct TcpFragment *fragment)
{

    const unsigned total_len = fragment->length + sizeof(struct rte_ether_hdr) +
                               sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) +
                               fragment->optlen * sizeof(uint32_t);

    // 1 ethhdr
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->s_addr.addr_bytes, srcmac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->d_addr.addr_bytes, dstmac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    // 2 iphdr
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(msg + sizeof(struct rte_ether_hdr));
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64; // ttl = 64
    ip->next_proto_id = IPPROTO_TCP;
    ip->src_addr = sip;
    ip->dst_addr = dip;

    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // 3 udphdr

    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(msg + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
    tcp->src_port = fragment->srcPort;
    tcp->dst_port = fragment->dstPort;
    tcp->sent_seq = htonl(fragment->seqnum);
    tcp->recv_ack = htonl(fragment->acknum);

    tcp->data_off = fragment->hdrlen_off;
    tcp->rx_win = fragment->windows;
    tcp->tcp_urp = fragment->tcp_urp;
    tcp->tcp_flags = fragment->tcp_flags;

    if (fragment->data != nullptr)
    {
        uint8_t *payload = (uint8_t *)(tcp + 1) + fragment->optlen * sizeof(uint32_t);
        rte_memcpy(payload, fragment->data, fragment->length);
    }

    tcp->cksum = 0;
    tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);

    return 0;
}
