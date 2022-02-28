// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/SocketsOps.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"
#include "muduo/net/Endian.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>  // snprintf
#include <sys/socket.h>
#include <sys/uio.h>  // readv
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace {

typedef struct sockaddr SA;


#if VALGRIND || defined (NO_ACCEPT4)
void setNonBlockAndCloseOnExec(int sockfd)
{
    // non-block
    /**
     * @comment fcntl
     * @link https://www.gnu.org/software/libc/manual/html_node/Open_002dtime-Flags.html
     * @brief
     * - prototype:  int fcntl(int fd, int cmd, ... );
     * - purpose: fcntl() performs one of the operations described below on the open file descriptor fd.  The operation is determined by cmd.
     * - flag:
     *      - F_GETFL: Return (as the function result) the file access mode and the file status flags; arg is ignored
     *      - F_SETFL: Set the file status flags to the value specified by arg.
     *          - O_NONBLOCK: This prevents open from blocking for a “long time” to open the file. This is only meaningful for some kinds of files,
     *                        usually devices such as serial ports; when it is not meaningful, it is harmless and ignored
     *
     *                        When possible, the file is opened in nonblocking mode. Neither the open() nor any subsequent I/O operations on the file descriptor
     *                        which is returned will cause the calling process to wait.
     *          - FD_CLOEXEC: If the FD_CLOEXEC bit is set, the file descriptor will automatically be closed during a successful execve(2).
     *                        (If the execve(2) fails, the file descriptor is left open.)  If the FD_CLOEXEC bit is not set,
     *                        the file descriptor will remain open across an execve(2)
     */
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);
    // FIXME check

    // close-on-exec
    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);
    // FIXME check

    (void)ret;
}
#endif

}  // namespace

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in6* addr)
{
    return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

struct sockaddr* sockets::sockaddr_cast(struct sockaddr_in6* addr)
{
    return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in* addr)
{
    return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_in* sockets::sockaddr_in_cast(const struct sockaddr* addr)
{
    return static_cast<const struct sockaddr_in*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_in6* sockets::sockaddr_in6_cast(const struct sockaddr* addr)
{
    return static_cast<const struct sockaddr_in6*>(implicit_cast<const void*>(addr));
}

int sockets::createNonblockingOrDie(sa_family_t family)
{
#if VALGRIND
    /**
     * @comment socket
     * @brief
     * - prototype: int socket(int domain, int type, int protocol);
     * - purpose: socket() creates an endpoint for communication and returns a file descriptor that refers to that endpoint.
     *            The file descriptor returned by a successful call will be the lowest-numbered file descriptor not currently open for the process.
     * - explain:
     *   - domain: The domain argument specifies a communication domain; this selects the protocol family which will be used for communication.
     *             These families are defined in <sys/socket.h>.  The formats currently understood by the Linux kernel include
     *     - AF_UNIX: Local communication
     *     - AF_LOCAL: Synonym for AF_UNIX
     *     - AF_INET: IPv4 Internet protocols
     *     - AF_INET6: IPv6 Internet protocols
     *   - type: The socket has the indicated type, which specifies the communication semantics.
     *     - SOCK_STREAM: Provides sequenced, reliable, two-way, connection-based byte streams.  An out-of-band data transmission mechanism may be supported.
     *     - SOCK_DGRAM: Supports datagrams (connectionless, unreliable messages of a fixed maximum length).
     *   - type: Since Linux 2.6.27, the type argument serves a second purpose: in addition to specifying a socket type,
     *           it may include the bitwise OR of any of the following values, to modify the behavior of socket()
     *     - SOCK_NONBLOCK: Set the O_NONBLOCK file status flag on the open file description (see open(2)) referred to by the new file descriptor.
     *     - SOCK_NONBLOCK:  Set the close-on-exec (FD_CLOEXEC) flag on the new file descriptor.
     *   - protocol: The protocol specifies a particular protocol to be used with the socket.
     *               Normally only a single protocol exists to support a particular socket type within a given protocol family, in which case protocol can be specified as 0.
     *               However, it is possible that many protocols may exist, in which case a particular protocol must be specified in this manner.
     *               The protocol number to use is specific to the “communication domain” in which communication is to take place;
     *     - IPPROTO_TCP: To set or get a TCP socket option, call getsockopt(2) to read or setsockopt(2) to write the option with the option level argument set to IPPROTO_TCP.
     */
    int sockfd = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_SYSFATAL << "sockets::createNonblockingOrDie";
    }

    setNonBlockAndCloseOnExec(sockfd);
#else
    int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_SYSFATAL << "sockets::createNonblockingOrDie";
    }
#endif
    return sockfd;
}

void sockets::bindOrDie(int sockfd, const struct sockaddr* addr)
{
    /**
     * @comment bind
     * @brief
     * - prototype: int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
     * - purpose: When a socket is created with socket(2), it exists in a name space (address family) but has no address assigned to it.
     *            bind() assigns the address specified by addr to the socket referred to by the file descriptor sockfd.  addrlen specifies the size,
     *            in bytes, of the address structure pointed to by addr. Traditionally, this operation is called “assigning a name to a socket”.
     *
     *            It is normally necessary to assign a local address using bind() before a SOCK_STREAM socket may receive connections (see accept(2))
     */
    int ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
    if (ret < 0) {
        LOG_SYSFATAL << "sockets::bindOrDie";
    }
}

void sockets::listenOrDie(int sockfd)
{
    /**
     * @comment listen
     * @brief
     * - prototype: int listen(int sockfd, int backlog);
     * - purpose: listen() marks the socket referred to by sockfd as a passive socket,
     *            that is, as a socket that will be used to accept incoming connection requests using accept(2).
     * - explain:
     *   - sockfd: The sockfd argument is a file descriptor that refers to a socket of type SOCK_STREAM or SOCK_SEQPACKET.
     *   - backlog:  The backlog argument defines the maximum length to which the queue of pending connections for sockfd may grow.
     *               If a connection request arrives when the queue is full, the client may receive an error with an indication of ECONNREFUSED or,
     *               if the underlying protocol supports retransmission, the request may be ignored so that a later reattempt at connection succeeds.
     *   - SOMAXCONN: If the backlog argument is greater than the value in /proc/sys/net/core/somaxconn, then it is silently capped to that value.
     *                Since Linux 5.4, the default in this file is 4096; in earlier kernels, the default value is 128.  In kernels before 2.4.25, this limit was a hard coded value,
     *                SOMAXCONN, with the value 128.
     */
    int ret = ::listen(sockfd, SOMAXCONN);
    if (ret < 0) {
        LOG_SYSFATAL << "sockets::listenOrDie";
    }
}

int sockets::accept(int sockfd, struct sockaddr_in6* addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof * addr);
#if VALGRIND || defined (NO_ACCEPT4)
    /**
     * @comment accept
     * @brief
     * - prototype: int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
     * - purpose: The accept() system call is used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET).
     *            It extracts the first connection request on the queue of pending connections for the listening socket, sockfd,
     *            creates a new connected socket, and returns a new file descriptor referring to that socket.  The newly created socket is not in the listening state.
     *            The original socket sockfd is unaffected by this call.
     * - explain:
     *    - sockfd: The argument sockfd is a socket that has been created with socket(2), bound to a local address with bind(2),
     *              and is listening for connections after a listen(2).
     *    - addr: The argument addr is a pointer to a sockaddr structure.  This structure is filled in with the address of the peer socket,
     *            as known to the communications layer.  The exact format of the address returned addr is determined by the socket's address family
     *            (see socket(2) and the respective protocol man pages). When addr is NULL, nothing is filled in; in this case, addrlen is not used, and should also be NULL.
     *    - addrlen: The addrlen argument is a value-result argument: the caller must initialize it to contain the size (in bytes) of the structure pointed to by addr;
     *               on return it will contain the actual size of the peer address.
     */
    int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
    setNonBlockAndCloseOnExec(connfd);
#else
    /**
     * @comment accept4
     * @brief
     * - prototype: int accept4(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen, int flags);
     * - purpose: If flags is 0, then accept4() is the same as accept().  The following values can be bitwise ORed in flags to obtain different behavior:
     *   - SOCK_NONBLOCK
     *   - SOCK_CLOEXEC
     */
    int connfd = ::accept4(sockfd, sockaddr_cast(addr),
                           &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
    if (connfd < 0) {
        int savedErrno = errno;
        LOG_SYSERR << "Socket::accept";
        switch (savedErrno) {
        case EAGAIN:
        case ECONNABORTED:
        case EINTR:
        case EPROTO: // ???
        case EPERM:
        case EMFILE: // per-process lmit of open file desctiptor ???
            // expected errors
            errno = savedErrno;
            break;
        case EBADF:
        case EFAULT:
        case EINVAL:
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case ENOTSOCK:
        case EOPNOTSUPP:
            // unexpected errors
            LOG_FATAL << "unexpected error of ::accept " << savedErrno;
            break;
        default:
            LOG_FATAL << "unknown error of ::accept " << savedErrno;
            break;
        }
    }
    return connfd;
}

int sockets::connect(int sockfd, const struct sockaddr* addr)
{
    /**
     * @comment connect
     * @brief
     * - prototype: int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
     * - purpose:  The connect() system call connects the socket referred to by the file descriptor sockfd to the address specified by addr.
     *             The addrlen argument specifies the size of addr.  The format of the address in addr is determined by the address space of the socket sockfd;
     *             see socket(2) for further details.
     */
    return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
}

ssize_t sockets::read(int sockfd, void *buf, size_t count)
{
    /**
     * @comment read
     * @brief
     * - prototype: ssize_t read(int fd, void *buf, size_t count);
     * - purpose: read() attempts to read up to count bytes from file descriptor fd into the buffer starting at buf.
     */
    return ::read(sockfd, buf, count);
}

ssize_t sockets::readv(int sockfd, const struct iovec *iov, int iovcnt)
{
    /**
     * @comment readv
     * @brief
     * - prototype: ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
     * - purpose: The readv() system call reads iovcnt buffers from the file associated with the file descriptor fd into the buffers described by iov ("scatter input").
     */
    return ::readv(sockfd, iov, iovcnt);
}

ssize_t sockets::write(int sockfd, const void *buf, size_t count)
{
    /**
     * @comment write
     * @brief
     * - prototype: ssize_t write(int fd, const void *buf, size_t count);
     * - purpose: write() writes up to count bytes from the buffer starting at buf to the file referred to by the file descriptor fd.
     * - return:  On success, the number of bytes written is returned.  On error, -1 is returned, and errno is set to indicate the error.
     */
    return ::write(sockfd, buf, count);
}

void sockets::close(int sockfd)
{
    /**
     * @comment close
     * @brief
     * - prototype: int close(int fd)
     * - purpose: close() closes a file descriptor, so that it no longer refers to any file and may be reused.  Any record locks (see fcntl(2)) held on the file
     *            it was associated with, and owned by the process, are removed (regardless of the file descriptor that was used to obtain the lock).
     */
    if (::close(sockfd) < 0) {
        LOG_SYSERR << "sockets::close";
    }
}

void sockets::shutdownWrite(int sockfd)
{
    /**
     * @comment shutdown
     * @brief
     * - prototytpe: int shutdown(int sockfd, int how);
     * - purpose: The shutdown() call causes all or part of a full-duplex connection on the socket associated with sockfd to be shut down.
     *            If how is SHUT_RD, further receptions will be disallowed.  If how is SHUT_WR, further transmissions will be disallowed.
     *            If how is SHUT_RDWR, further receptions and transmissions will be disallowed
     */
    if (::shutdown(sockfd, SHUT_WR) < 0) {
        LOG_SYSERR << "sockets::shutdownWrite";
    }
}

void sockets::toIpPort(char* buf, size_t size,
                       const struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET6) {
        buf[0] = '[';
        toIp(buf + 1, size - 1, addr);
        size_t end = ::strlen(buf);
        const struct sockaddr_in6* addr6 = sockaddr_in6_cast(addr);
        uint16_t port = sockets::networkToHost16(addr6->sin6_port);
        assert(size > end);
        snprintf(buf + end, size - end, "]:%u", port);
        return;
    }
    toIp(buf, size, addr);
    size_t end = ::strlen(buf);
    const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
    uint16_t port = sockets::networkToHost16(addr4->sin_port);
    assert(size > end);
    snprintf(buf + end, size - end, ":%u", port);
}

void sockets::toIp(char* buf, size_t size,
                   const struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET) {
        assert(size >= INET_ADDRSTRLEN);
        const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
        /**
         * @comment inet_ntop
         * @brief
         * - prototype: const char *inet_ntop(int af, const void *restrict src, char *restrict dst, socklen_t size);
         * - purpose: This function converts the network address structure src in the af address family into a character string.
         *            The resulting string is copied to the buffer pointed to by dst, which must be a non- null pointer.
         *            The caller specifies the number of bytes available in this buffer in the argument size.
         */
        ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(size));
    }
    else if (addr->sa_family == AF_INET6) {
        assert(size >= INET6_ADDRSTRLEN);
        const struct sockaddr_in6* addr6 = sockaddr_in6_cast(addr);
        ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, static_cast<socklen_t>(size));
    }
}

void sockets::fromIpPort(const char* ip, uint16_t port,
                         struct sockaddr_in* addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = hostToNetwork16(port);
    /**
     * @comment inet_pton
     * @brief
     * - prototype: int inet_pton(int af, const char *restrict src, void *restrict dst);
     * - purpose: This function converts the character string src into a network address structure in the af address family,
     *            then copies the network address structure to dst.  The af argument must be either AF_INET or AF_INET6.
     *            dst is written in network byte order.
     *   - AF_INET: src points to a character string containing an IPv4 network address in dotted-decimal format,
     *              "ddd.ddd.ddd.ddd", where ddd is a decimal number of up to three digits in the range 0 to 255.  The address is converted to a struct in_addr and copied to dst,
     *              which must be sizeof(struct in_addr) (4) bytes (32 bits) long.
     *   - AF_INET6: src points to a character string containing an IPv6 network address.  The address is converted to a struct in6_addr and copied to dst,
     *               which must be sizeof(struct in6_addr) (16) bytes (128 bits) long.  The allowed formats for IPv6 addresses follow these rules:
     */
    if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
        LOG_SYSERR << "sockets::fromIpPort";
    }
}

void sockets::fromIpPort(const char* ip, uint16_t port,
                         struct sockaddr_in6* addr)
{
    addr->sin6_family = AF_INET6;
    addr->sin6_port = hostToNetwork16(port);
    if (::inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0) {
        LOG_SYSERR << "sockets::fromIpPort";
    }
}

int sockets::getSocketError(int sockfd)
{
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);

    /**
     * @comment getsockopt
     * @brief
     * - prototype: int getsockopt(int sockfd, int level, int optname, void *restrict optval, socklen_t *restrict optlen);
     * - purpose: getsockopt() and setsockopt() manipulate options for the socket referred to by the file descriptor sockfd.  Options may exist at multiple protocol levels;
     *            they are always present at the uppermost socket level.
     *
     *            When manipulating socket options, the level at which the option resides and the name of the option must be specified.
     *            To manipulate options at the sockets API level, level is specified as SOL_SOCKET.  To manipulate options at any other level the protocol number of
     *            the appropriate protocol controlling the option is supplied.  For example, to indicate that an option is to be interpreted by the TCP protocol,
     *            level should be set to the protocol number of TCP;
     *
     *            The arguments optval and optlen are used to access option values for setsockopt().  For getsockopt() they identify a buffer in which the value for the 
     *            requested option(s) are to be returned. For getsockopt(), optlen is a value-result argument, initially containing the size of the buffer pointed to by optval, 
     *            and modified on return to indicate the actual size of the value returned.   If no option value is to be supplied or returned, optval may be NULL.
     *   - SOL_SOCKET: The socket options listed below can be set by using setsockopt(2) and read with getsockopt(2) with the socket level set to SOL_SOCKET for all sockets. 
     *                 Unless otherwise noted, optval is a pointer to an int.
     *   - SO_ERROR: Get and clear the pending socket error.  This socket option is read-only.  Expects an integer.
     */
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        return errno;
    }
    else {
        return optval;
    }
}

struct sockaddr_in6 sockets::getLocalAddr(int sockfd)
{
    struct sockaddr_in6 localaddr;
    memZero(&localaddr, sizeof localaddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
    /**
     * @comment getsockname
     * @brief
     * - prototype: int getsockname(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
     * - purpose: getsockname() returns the current address to which the socket sockfd is bound, in the buffer pointed to by addr.
     *            The addrlen argument should be initialized to indicate the amount of space (in bytes) pointed to by addr.
     *            On return it contains the actual size of the socket address.
     */
    if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0) {
        LOG_SYSERR << "sockets::getLocalAddr";
    }
    return localaddr;
}

struct sockaddr_in6 sockets::getPeerAddr(int sockfd)
{
    struct sockaddr_in6 peeraddr;
    memZero(&peeraddr, sizeof peeraddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
    /**
     * @comment getpeername
     * @brief
     * - prototype: int getpeername(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
     * - purpose: getpeername() returns the address of the peer connected to the socket sockfd, in the buffer pointed to by addr.  
     *            The addrlen argument should be initialized to indicate the amount of space pointed to by addr.  On return it contains the actual size of 
     *            the name returned (in bytes).  The name is truncated if the buffer provided is too small.
     *
     *            The returned address is truncated if the buffer provided is too small; in this case, addrlen will return a value greater than was supplied to the call.
     */
    if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0) {
        LOG_SYSERR << "sockets::getPeerAddr";
    }
    return peeraddr;
}

bool sockets::isSelfConnect(int sockfd)
{
    struct sockaddr_in6 localaddr = getLocalAddr(sockfd);
    struct sockaddr_in6 peeraddr = getPeerAddr(sockfd);
    if (localaddr.sin6_family == AF_INET) {
        const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
        const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
        return laddr4->sin_port == raddr4->sin_port
               && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
    }
    else if (localaddr.sin6_family == AF_INET6) {
        return localaddr.sin6_port == peeraddr.sin6_port
               && memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof localaddr.sin6_addr) == 0;
    }
    else {
        return false;
    }
}
