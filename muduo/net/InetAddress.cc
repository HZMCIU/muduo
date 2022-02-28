// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/InetAddress.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Endian.h"
#include "muduo/net/SocketsOps.h"

#include <netdb.h>
#include <netinet/in.h>

// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };

using namespace muduo;
using namespace muduo::net;

static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6),
    "InetAddress is same size as sockaddr_in6");

/**
 * @comment offsetof 
 * @link https://en.cppreference.com/w/cpp/types/offsetof
 * @brief
 * - prototype: #define offsetof(type, member) //implementation-defined
 * - purpose: The macro offsetof expands to an integral constant expression of type std::size_t, the value of which is the offset, in bytes, 
 *            from the beginning of an object of specified type to its specified subobject, including padding if any.
 */

static_assert(offsetof(sockaddr_in, sin_family) == 0, "sin_family offset 0");
static_assert(offsetof(sockaddr_in6, sin6_family) == 0, "sin6_family offset 0");
static_assert(offsetof(sockaddr_in, sin_port) == 2, "sin_port offset 2");
static_assert(offsetof(sockaddr_in6, sin6_port) == 2, "sin6_port offset 2");

InetAddress::InetAddress(uint16_t portArg, bool loopbackOnly, bool ipv6)
{
    static_assert(offsetof(InetAddress, addr6_) == 0, "addr6_ offset 0");
    static_assert(offsetof(InetAddress, addr_) == 0, "addr_ offset 0");
    if (ipv6) {
        memZero(&addr6_, sizeof addr6_);
        addr6_.sin6_family = AF_INET6;
        in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
        addr6_.sin6_addr = ip;
        addr6_.sin6_port = sockets::hostToNetwork16(portArg);
    } else {
        memZero(&addr_, sizeof addr_);
        addr_.sin_family = AF_INET;
        in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
        addr_.sin_addr.s_addr = sockets::hostToNetwork32(ip);
        addr_.sin_port = sockets::hostToNetwork16(portArg);
    }
}

InetAddress::InetAddress(StringArg ip, uint16_t portArg, bool ipv6)
{
    if (ipv6 || strchr(ip.c_str(), ':')) {
        memZero(&addr6_, sizeof addr6_);
        sockets::fromIpPort(ip.c_str(), portArg, &addr6_);
    } else {
        memZero(&addr_, sizeof addr_);
        sockets::fromIpPort(ip.c_str(), portArg, &addr_);
    }
}

string InetAddress::toIpPort() const
{
    char buf[64] = "";
    sockets::toIpPort(buf, sizeof buf, getSockAddr());
    return buf;
}

string InetAddress::toIp() const
{
    char buf[64] = "";
    sockets::toIp(buf, sizeof buf, getSockAddr());
    return buf;
}

uint32_t InetAddress::ipv4NetEndian() const
{
    assert(family() == AF_INET);
    return addr_.sin_addr.s_addr;
}

uint16_t InetAddress::port() const
{
    return sockets::networkToHost16(portNetEndian());
}

static __thread char t_resolveBuffer[64 * 1024];

bool InetAddress::resolve(StringArg hostname, InetAddress* out)
{
    assert(out != NULL);
    /**
     * @comment struct hostent
     * @link 
     * - https://www.gnu.org/software/libc/manual/html_node/Host-Names.html
     * - https://man7.org/linux/man-pages/man3/gethostbyname.3.html
     * @brief
     * - prototype:
     *      struct hostent {
     *         char  *h_name;       // This is the “official” name of the host.     
     *         char **h_aliases;    // These are alternative names for the host, represented as a null-terminated vector of strings.
     *         int    h_addrtype;   // This is the host address type; in practice, its value is always either AF_INET or AF_INET6, with the latter being used for IPv6 hosts. 
     *                              // In principle other kinds of addresses could be represented in the database as well as Internet addresses; 
     *                              // if this were done, you might find a value in this field other than AF_INET or AF_INET6. See Socket Addresses.
     *         int    h_length;     // This is the length, in bytes, of each address. 
     *         char **h_addr_list;  // This is the vector of addresses for the host. (Recall that the host might be connected to multiple networks 
     *                              // and have different addresses on each one.) The vector is terminated by a null pointer. 
     *     }
     *     #define h_addr h_addr_list[0]
     */
    struct hostent hent;
    struct hostent* he = NULL;
    int herrno = 0;
    memZero(&hent, sizeof(hent));

    /**
     * @comment gethostbyname_r
     * @brief
     * - prototype: int gethostbyname_r(const char *restrict name, struct hostent *restrict ret, char *restrict buf, size_t buflen, struct hostent **restrict result, 
     *              int *restrict h_errnop);
     * - purpose:  The gethostbyname() function returns a structure of type hostent for the given host name.  
     *             Here name is either a hostname or an IPv4 address in standard dot notation (as for inet_addr(3)).  If name is an IPv4 address, 
     *             no lookup is performed and gethostbyname() simply copies name into the h_name field and its struct in_addr equivalent into 
     *             the h_addr_list[0] field of the returned hostent structure.  If name doesn't end in a dot and the environment variable HOSTALIASES is set, 
     *             the alias file pointed to by HOSTALIASES will first be searched for name (see hostname(7) for the file format). 
     *             The current domain and its parents are searched unless name ends in a dot.
     */
    int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
    if (ret == 0 && he != NULL) {
        assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
        out->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
        return true;
    } else {
        if (ret) {
            LOG_SYSERR << "InetAddress::resolve";
        }
        return false;
    }
}

void InetAddress::setScopeId(uint32_t scope_id)
{
    if (family() == AF_INET6) {
        addr6_.sin6_scope_id = scope_id;
    }
}
