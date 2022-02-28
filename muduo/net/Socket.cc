// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/Socket.h"

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h> // snprintf

using namespace muduo;
using namespace muduo::net;

Socket::~Socket()
{
    sockets::close(sockfd_);
}

/**
 * @comment struct tcp_info
 * @link https://linuxgazette.net/136/pfeiffer.html
 * @brief
 * - prototype:
 *
 * struct tcp_info
 * {
 *   uint8_t	tcpi_state;
 *   uint8_t	tcpi_ca_state;
 *   uint8_t	tcpi_retransmits;
 *   uint8_t	tcpi_probes;
 *   uint8_t	tcpi_backoff;
 *   uint8_t	tcpi_options;
 *   uint8_t	tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;
 * 
 *   uint32_t	tcpi_rto;
 *   uint32_t	tcpi_ato;
 *   uint32_t	tcpi_snd_mss;
 *   uint32_t	tcpi_rcv_mss;
 * 
 *   uint32_t	tcpi_unacked;
 *   uint32_t	tcpi_sacked;
 *   uint32_t	tcpi_lost;
 *   uint32_t	tcpi_retrans;
 *   uint32_t	tcpi_fackets;
 * 
 *   // Times.
 *   uint32_t	tcpi_last_data_sent;
 *   uint32_t	tcpi_last_ack_sent;	// Not remembered, sorry. 
 *   uint32_t	tcpi_last_data_recv;
 *   uint32_t	tcpi_last_ack_recv;
 * 
 *   // Metrics.
 *   uint32_t	tcpi_pmtu;
 *   uint32_t	tcpi_rcv_ssthresh;
 *   uint32_t	tcpi_rtt;
 *   uint32_t	tcpi_rttvar;
 *   uint32_t	tcpi_snd_ssthresh;
 *   uint32_t	tcpi_snd_cwnd;
 *   uint32_t	tcpi_advmss;
 *   uint32_t	tcpi_reordering;
 * 
 *   uint32_t	tcpi_rcv_rtt;
 *   uint32_t	tcpi_rcv_space;
 * 
 *   uint32_t	tcpi_total_retrans;
 * };
 */
bool Socket::getTcpInfo(struct tcp_info* tcpi) const
{
    socklen_t len = sizeof(*tcpi);
    memZero(tcpi, len);
    /**
     * @comment getsockopt
     * @brief
     * - prototype: int getsockopt(int sockfd, int level, int optname, void *restrict optval, socklen_t *restrict optlen);
     * - macros:
     *   - SOL_TCP: indicate TCP level, defined in /usr/include/netinet/tcp.h
     *   - TCP_INFO: indicate TCP information, defined in /usr/include/netinet/tcp.h
     */
    return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::getTcpInfoString(char* buf, int len) const
{
    struct tcp_info tcpi;
    bool ok = getTcpInfo(&tcpi);
    if (ok) {
        snprintf(buf, len, "unrecovered=%u "
                           "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
                           "lost=%u retrans=%u rtt=%u rttvar=%u "
                           "sshthresh=%u cwnd=%u total_retrans=%u",
            tcpi.tcpi_retransmits, // Number of unrecovered [RTO] timeouts
            tcpi.tcpi_rto,         // Retransmit timeout in usec
            tcpi.tcpi_ato,         // Predicted tick of soft clock in usec
            tcpi.tcpi_snd_mss,
            tcpi.tcpi_rcv_mss,
            tcpi.tcpi_lost,    // Lost packets
            tcpi.tcpi_retrans, // Retransmitted packets out
            tcpi.tcpi_rtt,     // Smoothed round trip time in usec
            tcpi.tcpi_rttvar,  // Medium deviation
            tcpi.tcpi_snd_ssthresh,
            tcpi.tcpi_snd_cwnd,
            tcpi.tcpi_total_retrans); // Total retransmits for entire connection
    }
    return ok;
}

void Socket::bindAddress(const InetAddress& addr)
{
    sockets::bindOrDie(sockfd_, addr.getSockAddr());
}

void Socket::listen()
{
    sockets::listenOrDie(sockfd_);
}

int Socket::accept(InetAddress* peeraddr)
{
    struct sockaddr_in6 addr;
    memZero(&addr, sizeof addr);
    int connfd = sockets::accept(sockfd_, &addr);
    if (connfd >= 0) {
        peeraddr->setSockAddrInet6(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    sockets::shutdownWrite(sockfd_);
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
        &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
        &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}

void Socket::setReusePort(bool on)
{
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
        &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
        LOG_SYSERR << "SO_REUSEPORT failed.";
    }
#else
    if (on) {
        LOG_ERROR << "SO_REUSEPORT is not supported.";
    }
#endif
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
        &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}
