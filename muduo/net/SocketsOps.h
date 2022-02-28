// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_SOCKETSOPS_H
#define MUDUO_NET_SOCKETSOPS_H

#include <arpa/inet.h>

namespace muduo {
namespace net {
namespace sockets {

///
/// Creates a non-blocking socket file descriptor,
/// abort if any error.
/**
 * @comment sa_family_t
 * @link https://pubs.opengroup.org/onlinepubs/009604599/basedefs/sys/socket.h.html
 * @brief: identifies the protocol's address family.
 */
int createNonblockingOrDie(sa_family_t family);

/**
 * @comment struct sockaddr
 * @link 
 * - https://stackoverflow.com/questions/7414943/understanding-struct-sockaddr
 * - https://man7.org/linux/man-pages/man2/bind.2.html
 * @brief 
 * - prototype:
 *   struct sockaddr {
 *      sa_family_t sa_family;
 *      char        sa_data[14];
 *   }
 * - purpose: sockaddr is used as the base of a set of address structures that act like a discriminated union, see the Beej guide to networking. 
 *            You generally look at the sa_family and then cast to the appropriate address family's specific address structure.
 */
int  connect(int sockfd, const struct sockaddr* addr);
void bindOrDie(int sockfd, const struct sockaddr* addr);
void listenOrDie(int sockfd);
int  accept(int sockfd, struct sockaddr_in6* addr);
ssize_t read(int sockfd, void *buf, size_t count);
ssize_t readv(int sockfd, const struct iovec *iov, int iovcnt);
ssize_t write(int sockfd, const void *buf, size_t count);
void close(int sockfd);
void shutdownWrite(int sockfd);

void toIpPort(char* buf, size_t size,
              const struct sockaddr* addr);
void toIp(char* buf, size_t size,
          const struct sockaddr* addr);

void fromIpPort(const char* ip, uint16_t port,
                struct sockaddr_in* addr);
void fromIpPort(const char* ip, uint16_t port,
                struct sockaddr_in6* addr);

int getSocketError(int sockfd);

/**
 * @comment sockaddr_in
 * @brief
 * - prototype:
 *   struct sockaddr_in {
 *      sa_family_t    sin_family; // address family: AF_INET 
 *      in_port_t      sin_port;   // port in network byte order 
 *      struct in_addr sin_addr;   // internet address 
 *   };
 *
 *   // Internet address 
 *   struct in_addr {
 *      uint32_t       s_addr;     // address in network byte order 
 *   };
 */

const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
/**
 * @comment sockaddr_in6 
 * @brief
 * - prototype:
 *   struct sockaddr_in6 {
 *      sa_family_t     sin6_family;   // AF_INET6
 *      in_port_t       sin6_port;     // port number
 *      uint32_t        sin6_flowinfo; // IPv6 flow information
 *      struct in6_addr sin6_addr;     // IPv6 address
 *      uint32_t        sin6_scope_id; // Scope ID (new in 2.4)
 *      };
 * - explain:
 *      - sin6_family is always set to AF_INET6;
 *      - sin6_port is the protocol port (see sin_port in ip(7))
 *      - sin6_flowinfo is the IPv6 flow identifier
 *      - sin6_addr is the 128-bit IPv6 address.
 *      - sin6_scope_id is an ID depending on the scope of the address.
 *      - Linux supports it only for link-local addresses, in that case sin6_scope_id contains the interface index
 */
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
struct sockaddr* sockaddr_cast(struct sockaddr_in6* addr);
const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);
const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr* addr);

struct sockaddr_in6 getLocalAddr(int sockfd);
struct sockaddr_in6 getPeerAddr(int sockfd);
bool isSelfConnect(int sockfd);

}  // namespace sockets
}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_SOCKETSOPS_H
