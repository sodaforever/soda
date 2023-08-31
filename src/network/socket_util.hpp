#pragma once

// SocketUtil tool - create TCP server/client; set multiplexing port and other options; NIO/BIO; support IPv4/IPv6; NO SIGPIPE

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <netdb.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <time.h>
#include <sys/sendfile.h>

#include "../general/util.hpp"

namespace soda
{
    struct AddrInfo
    {
        std::string addr;
        uint16_t port;
    };

    struct ConnInfo
    {
        int32_t fd;
        std::string addr;
        uint16_t port;
    };

    class SocketUtil : Noncopyable
    {

    private:
        std::string m_addr;
        uint16_t m_port;
        // SocketUtil transmission data type streaming SOCK_STREAM; datagram is SOCK_DGRAM
        int32_t m_socktype;
        // Protocol 0 is TCP for stream and UDP for datagram transport;
        int32_t m_protocol;
        int32_t m_sockfd;
        sockaddr_storage m_sockaddr;
        sockaddr *m_sockaddr_ptr;
        socklen_t m_sockaddr_size;

    public:
        using conn_info_ptr = std::shared_ptr<ConnInfo>;
        SocketUtil();
        SocketUtil(const std::string &addr, uint16_t port, int socktype, int protocol);
        ~SocketUtil();

        // -1 if failed
        int start_tcp_server(const std::string &addr, uint16_t port);
        // -1 if failed
        int start_tcp_server();

        // -1 if failed
        int start_udp_server(const std::string &addr, uint16_t port);
        // -1 if failed
        int start_udp_server();

        // -1 if failed
        int start_tcp_client(const std::string &addr, uint16_t port);
        // -1 if failed
        int start_tcp_client();

        void stop();

        void set_addr(const std::string &addr);
        void set_port(uint16_t port);
        // Set the data transfer type, the default stream is SOCK_STREAM; the datagram is SOCK_DGRAM
        void set_socktype(int socktype);
        // Set the data transfer protocol SOCK_STREAM defaults to TCP; OCK_DGRAM defaults to UDP
        void set_protocol(int protocol);

        int get_sockfd();
        std::string get_addr();
        uint16_t get_port();

        // -1 if failed
        int set_reuseaddr();

        // -1 if failed
        int set_reuseport();

        // 1 is on, 0 is off, how many seconds to start monitoring after being idle, and the time interval for each detection, how many times at most
        // -1 if failed
        int set_keepalive(bool enable, int idle, int interval, int maxpkt);

        // set to close the Nagle algorithm, no longer waiting for small packets, reducing delay
        // -1 if failed
        int set_nodelay();

        // if enabled, the data to be sent will be buffered as much as possible
        // -1 if failed
        int set_cork(int flag);

        // set up IPv4/IPv6 dual stack. when creating an IPv6 socket, set the IPV6_V6ONLY option to 0, which ensures that the socket can accept both IPv4 and IPv6 connections
        // -1 if failed
        int set_not_IPv6_only();

        // Set up non-blocking IO
        // -1 if failed
        int set_nonblocking(int fd, bool non_blocking = true);

        // Set receive buffer size
        // -1 if failed
        int set_rcvbuf(size_t size);

        // Set send buffer size
        // -1 if failed
        int set_sndbuf(size_t size);

        // -1 if failed
        int set_read_timeout(int32_t seconds);

        // -1 if failed
        int set_write_timeout(int32_t seconds);

        // true for non-blocking IO, false for blocking IO;
        bool is_nonblocking(int fd);

        // nullptr if failed, conn->fd == -1 if errno == EAGAIN
        conn_info_ptr accept();

        // -1 if failed or disconnected; received length on success; 0 if there is no data to read
        int recv(uint32_t fd, void *dst, size_t size, int flags = 0);

        // -1 if failed or disconnected; received length on success; 0 if there is no data to read
        int recv_from(uint32_t fd, void *dst, size_t size, AddrInfo *ai, int flags = 0);

        // close sockfd, just dereference, when all references are closed, it will be really closed, if it is a connection, send fin
        // -1 if failed
        int close_sockfd(int fd);

        // close the connection, you can wait for unreached data, no need to close fd
        // -1 if failed, there is data to read > 0;
        int close_conn(int fd, void *dst = nullptr, size_t size = 0);

        int connect_sock();

        // -1 if failed; success returns the amount of data sent
        int send(uint32_t fd, const void *src, size_t size, int flags = 0);

        // -1 if failed; success returns the amount of data sent
        int send_to(const void *src, size_t size, const std::string &addr, uint16_t port, int flags);

        // -1 if failed; success returns the amount of data sent
        int sendfile(uint32_t srcfd, uint32_t dstfd, off_t *offset, size_t count);

    private:
        // -1 if failed
        int resolve_addr(const std::string &addr,
                         const std::string &service,
                         addrinfo **dst,
                         int socktype = SOCK_STREAM,
                         int protocol = 0,
                         int flags = 0);

        // -1 if failed
        int create_sock(bool is_server);

        // -1 if failed
        int bind_sock();

        // -1 if failed
        int listen_sock();

        // -1 if failed, length for success, ignore EINTR
        inline int recv_ign_EINTR(uint32_t fd, void *dst, size_t size, int flags);

        // -1 if failed, length for success, ignore EINTR
        inline int recv_ign_EINTR_from(uint32_t fd, void *dst, size_t size, int flags, sockaddr *addr, socklen_t *len);

        // -1 if failed, length for success, ignore EINTR
        inline int send_ign_EINTR(uint32_t fd, const void *src, size_t size, int flags);

        // -1 if failed, length for success, ignore EINTR
        inline int send_ign_EINTR_to(int fd, const void *src, size_t size, int flags, const sockaddr *addr, socklen_t len);

        // sockaddr -> AddrInfo
        AddrInfo to_addrinfo(sockaddr *sockaddr);

        inline int can_continue() const;
    };

    inline int SocketUtil::can_continue() const
    {
        return (EWOULDBLOCK == errno || EAGAIN == errno) ? 0 : -1;
    }

    SocketUtil::SocketUtil() : m_addr(""),
                               m_port(0),
                               m_socktype(-1),
                               m_protocol(-1),
                               m_sockfd(-1),
                               m_sockaddr{},
                               m_sockaddr_ptr(nullptr),
                               m_sockaddr_size(0) {}

    SocketUtil::SocketUtil(const std::string &addr, uint16_t port, int socktype, int protocol) : m_addr(addr),
                                                                                                 m_port(port),
                                                                                                 m_socktype(socktype),
                                                                                                 m_protocol(protocol),
                                                                                                 m_sockfd(-1),
                                                                                                 m_sockaddr{},
                                                                                                 m_sockaddr_ptr(nullptr),
                                                                                                 m_sockaddr_size(0) {}
    SocketUtil::~SocketUtil()
    {
        stop();
    }

    void SocketUtil::set_addr(const std::string &addr) { m_addr = addr; }

    void SocketUtil::set_port(uint16_t port) { m_port = port; }

    void SocketUtil::set_socktype(int socktype) { m_socktype = socktype; }

    void SocketUtil::set_protocol(int prorocol) { m_protocol = prorocol; }

    int SocketUtil::get_sockfd() { return m_sockfd; }

    std::string SocketUtil::get_addr() { return m_addr; }

    uint16_t SocketUtil::get_port() { return m_port; }

    void SocketUtil::stop()
    {
        if (-1 != m_sockfd)
        {
            close_sockfd(m_sockfd);
        }
        memset(&m_sockaddr, 0, sizeof(m_sockaddr));
        m_sockaddr_ptr = nullptr;
        m_sockaddr_size = 0;
    }

    int SocketUtil::start_tcp_server(const std::string &addr, uint16_t port)
    {
        set_addr(addr);
        set_port(port);
        return start_tcp_server();
    }

    int SocketUtil::start_tcp_server()
    {
        set_socktype(SOCK_STREAM);
        set_protocol(0);

        if (create_sock(true) == -1 ||
            set_not_IPv6_only() == -1 ||
            set_reuseaddr() == -1 ||
            bind_sock() == -1 ||
            listen_sock() == -1)
        {
            return -1;
        }
        return 0;
    }

    int SocketUtil::start_tcp_client(const std::string &addr, uint16_t port)
    {
        set_addr(addr);
        set_port(port);
        return start_tcp_client();
    }

    int SocketUtil::start_tcp_client()
    {
        set_socktype(SOCK_STREAM);
        set_protocol(0);

        if (create_sock(false) == -1 ||
            connect_sock() == -1)
        {
            return -1;
        }
        return 0;
    }

    int SocketUtil::start_udp_server(const std::string &addr, uint16_t port)
    {
        set_addr(addr);
        set_port(port);
        return start_udp_server();
    }

    int SocketUtil::start_udp_server()
    {
        set_socktype(SOCK_DGRAM);
        set_protocol(0);

        if (create_sock(true) == -1 ||
            set_not_IPv6_only() == -1 ||
            set_reuseaddr() == -1 ||
            bind_sock() == -1)
        {
            return -1;
        }
        return 0;
    }

    int SocketUtil::resolve_addr(const std::string &addr,
                                 const std::string &service,
                                 addrinfo **dst,
                                 int socktype,
                                 int protocol,
                                 int flags)
    {
        addrinfo hints{};
        // IPv4/IPv6
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = socktype;
        hints.ai_protocol = protocol;
        hints.ai_flags = flags;

        int ret = getaddrinfo(addr.c_str(), service.c_str(), &hints, dst);
        if (0 != ret)
        {
            perror("resolve addr failed");
            return -1;
        }

        return 0;
    }

    int SocketUtil::create_sock(bool is_server)
    {
        // AI_PASSIVE for server to listen on all addrs
        int flags = is_server ? AI_PASSIVE : 0;
        addrinfo *dst;
        int ret = resolve_addr(m_addr, std::to_string(m_port), &dst, m_socktype, m_protocol, flags);
        if (0 != ret)
        {
            perror("create sock failed");
            return -1;
        }

        struct addrinfo *p;

        // find one
        for (p = dst; p != nullptr; p = p->ai_next)
        {
            m_sockfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (-1 == m_sockfd)
            {
                continue;
            }
            break;
        }

        if (!p)
        {
            perror("create sock failed");
            freeaddrinfo(dst);
            return -1;
        }

        // get sockaddr
        memcpy(&m_sockaddr, p->ai_addr, p->ai_addrlen);
        m_sockaddr_ptr = reinterpret_cast<sockaddr *>(&m_sockaddr);
        // different size for IPv4/IPv6
        m_sockaddr_size = p->ai_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

        freeaddrinfo(dst);
        return 0;
    }

    int SocketUtil::set_reuseaddr()
    {
        int optval = 1;
        if (-1 == setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
        {
            perror("set SOL_SOCKET SO_REUSEADDR failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::set_reuseport()
    {
        int optval = 1;
        if (-1 == setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)))
        {
            perror("set SOL_SOCKET SO_REUSEPORT failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::set_not_IPv6_only()
    {
        if (m_sockaddr.ss_family == AF_INET6)
        {
            int optval = 0;
            if (-1 == setsockopt(m_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval)))
            {
                perror("set not IPv6 only failed");
                return -1;
            }
        }
        return 0;
    }

    int SocketUtil::set_keepalive(bool enable, int idle, int interval, int maxpkt)
    {
        if (-1 == setsockopt(m_sockfd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)))
        {
            perror("set SOL_SOCKET SO_KEEPALIVE failed");
            return -1;
        }

        if (1 == enable)
        {
            int ret = -1;
            ret = setsockopt(m_sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
            ret = setsockopt(m_sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
            ret = setsockopt(m_sockfd, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(maxpkt));
            if (-1 == ret)
            {
                perror("set SOL_SOCKET SO_KEEPALIVE failed");
                return -1;
            }
        }
        return 0;
    }

    int SocketUtil::set_nodelay()
    {
        int optval = 1;
        if (-1 == setsockopt(m_sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)))
        {
            perror("set TCP_NODELAY failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::set_cork(int flag)
    {
        if (-1 == setsockopt(m_sockfd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)))
        {
            perror("set TCP_CORK failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::set_rcvbuf(size_t size)
    {
        if (-1 == setsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)))
        {
            perror("set SO_RCVBUF failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::set_sndbuf(size_t size)
    {
        if (-1 == setsockopt(m_sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)))
        {
            perror("set SO_SNDBUF failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::set_read_timeout(int32_t seconds)
    {
        struct timeval timeout;
        timeout.tv_sec = seconds;
        timeout.tv_usec = 0;

        // Set read timeout
        if (-1 == setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)))
        {
            perror("set SO_RCVTIMEO failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::set_write_timeout(int32_t seconds)
    {
        struct timeval timeout;
        timeout.tv_sec = seconds;
        timeout.tv_usec = 0;

        // Set read timeout
        if (-1 == setsockopt(m_sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)))
        {
            perror("set SO_RCVTIMEO failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::set_nonblocking(int fd, bool non_blocking)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (-1 == flags)
        {
            perror("set nonblocking failed");
            return -1;
        }

        if (non_blocking)
        {
            flags |= O_NONBLOCK;
        }
        else
        {
            flags &= ~O_NONBLOCK;
        }

        flags = fcntl(fd, F_SETFL, flags);
        if (-1 == flags)
        {
            perror("set nonblocking failed");
            return -1;
        }

        return 0;
    }

    bool SocketUtil::is_nonblocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL);
        if (-1 == flags)
        {
            perror("get nonblocking flags failed");
            return false;
        }

        if (flags & O_NONBLOCK)
        {
            return true;
        }

        return false;
    }

    int SocketUtil::bind_sock()
    {
        if (-1 == ::bind(m_sockfd, m_sockaddr_ptr, m_sockaddr_size))
        {
            perror("bind sock failed");
            return -1;
        }
        return 0;
    }

    int SocketUtil::listen_sock()
    {
        if (-1 == ::listen(m_sockfd, SOMAXCONN))
        {
            perror("listen failed");
            return -1;
        }
        return 0;
    }

    AddrInfo SocketUtil::to_addrinfo(sockaddr *sockaddr)
    {
        char addr_c[INET6_ADDRSTRLEN];
        uint16_t port = 0;
        if (AF_INET == sockaddr->sa_family)
        {
            sockaddr_in *addr_4 = reinterpret_cast<sockaddr_in *>(&sockaddr);
            inet_ntop(AF_INET, &addr_4->sin_addr, addr_c, sizeof(addr_c));
            port = ntohs(addr_4->sin_port);
        }
        else if (AF_INET6 == sockaddr->sa_family)
        {
            sockaddr_in6 *addr_6 = reinterpret_cast<sockaddr_in6 *>(&sockaddr);
            inet_ntop(AF_INET6, &addr_6->sin6_addr, addr_c, sizeof(addr_c));
            port = ntohs(addr_6->sin6_port);
        }
        return {std::string(addr_c), port};
    }

    SocketUtil::conn_info_ptr SocketUtil::accept()
    {
        sockaddr_storage addr;
        sockaddr *addr_ptr = reinterpret_cast<sockaddr *>(&addr);
        socklen_t addr_size = static_cast<socklen_t>(sizeof(sockaddr_storage));

        int32_t fd;
        do
        {
            fd = ::accept(m_sockfd, addr_ptr, &addr_size);
        } while (-1 == fd && EINTR == errno);

        if (-1 == fd)
        {
            if (0 == can_continue())
            {
                return std::make_shared<ConnInfo>(ConnInfo{-1, "", 0});
            }
            perror("accept failed");
            return nullptr;
        }

        AddrInfo ai = to_addrinfo(addr_ptr);
        return std::make_shared<ConnInfo>(ConnInfo{fd, ai.addr, ai.port});
    }

    int SocketUtil::recv(uint32_t fd, void *dst, size_t size, int flags)
    {
        int ret = recv_ign_EINTR(fd, dst, size, flags);
        if (ret > 0)
        {
            return ret;
        }
        if (0 == ret)
        {
            perror("recv failed");
            return -1;
        }

        perror("recv failed");
        return can_continue();
    }

    int SocketUtil::recv_from(uint32_t fd, void *dst, size_t size, AddrInfo *ai, int flags)
    {
        sockaddr_storage addr;
        sockaddr *addr_ptr = reinterpret_cast<sockaddr *>(&addr);
        socklen_t addr_size = static_cast<socklen_t>(sizeof(sockaddr_storage));

        int ret = recv_ign_EINTR_from(fd, dst, size, flags, addr_ptr, &addr_size);

        if (ret >= 0)
        {
            *ai = to_addrinfo(addr_ptr);
            return ret;
        }
        perror("recv failed");
        return can_continue();
    }

    int SocketUtil::close_sockfd(int fd)
    {
        int ret = close(fd);
        if (-1 == ret)
        {
            perror("close sockfd failed");
            return -1;
        }
        return ret;
    }

    int SocketUtil::close_conn(int fd, void *dst, size_t size)
    {
        if (shutdown(fd, SHUT_WR) == -1)
        {
            perror("shutdown sockfd failed");
        }

        int ret = 0;
        if (nullptr != dst)
        {
            ret = recv(fd, dst, size);
            read(fd, dst, size);
            if (-1 == ret)
            {
                perror("recv failed");
                return -1;
            }
        }

        if (shutdown(fd, SHUT_RD) == -1)
        {
            perror("shutdown sockfd failed");
        }
        if (close(fd) == -1)
        {
            perror("close sockfd failed");
        }

        return ret;
    }

    inline int SocketUtil::recv_ign_EINTR(uint32_t fd, void *dst, size_t size, int flags)
    {
        int ret = -1;
        do
        {
            ret = ::recv(fd, dst, size, flags);
        } while (-1 == ret && EINTR == errno);
        return ret;
    }

    inline int SocketUtil::recv_ign_EINTR_from(uint32_t fd, void *dst, size_t size, int flags, sockaddr *addr, socklen_t *len)
    {
        int ret = -1;
        do
        {
            ret = ::recvfrom(fd, dst, size, flags, addr, len);
        } while (-1 == ret && EINTR == errno);
        return ret;
    }

    inline int SocketUtil::send_ign_EINTR(uint32_t fd, const void *src, size_t size, int flags)
    {
        if (!src || 0 == size)
        {
            return 0;
        }

        int ret = -1;
        do
        {
            ret = ::send(fd, src, size, flags);
        } while (-1 == ret && errno != EINTR);
        return ret;
    }

    inline int SocketUtil::send_ign_EINTR_to(int fd, const void *src, size_t size, int flags, const sockaddr *addr, socklen_t len)
    {
        if (!src || 0 == size)
        {
            return 0;
        }

        int ret = -1;
        do
        {
            ret = ::sendto(fd, src, size, flags, addr, len);
        } while (-1 == ret && errno != EINTR);
        return ret;
    }

    int SocketUtil::send(uint32_t fd, const void *src, size_t size, int flags)
    {
        // shield SIGPIPE, expect -1 and EPIPE
        flags |= MSG_NOSIGNAL;
        int ret = send_ign_EINTR(fd, src, size, flags);
        if (ret >= 0)
        {
            return ret;
        }

        perror("send failed");
        return can_continue();
    }

    int SocketUtil::send_to(const void *src, size_t size, const std::string &addr, uint16_t port, int flags)
    {
        addrinfo *dst;
        int ret = resolve_addr(addr, std::to_string(port), &dst, SOCK_DGRAM, 0, 0);
        if (-1 == ret)
        {
            perror("send failed");
            return ret;
        }

        sockaddr *sockaddr_ptr = reinterpret_cast<sockaddr *>(dst->ai_addr);
        socklen_t socklen = sockaddr_ptr->sa_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

        ret = send_ign_EINTR_to(m_sockfd, src, size, flags, sockaddr_ptr, socklen);

        freeaddrinfo(dst);

        if (ret >= 0)
        {
            return ret;
        }
        perror("send failed");
        return can_continue();
    }

    int SocketUtil::sendfile(uint32_t dstfd, uint32_t srcfd, off_t *offset, size_t size)
    {
        size_t sent_size = 0;
        while (sent_size < size)
        {
            ssize_t ret = ::sendfile(dstfd, srcfd, offset, size - sent_size);
            if (-1 == ret)
            {
                if (errno == EINTR || 0 == can_continue())
                {
                    continue;
                }
                perror("sendfile failed");
                return -1;
            }

            sent_size += ret;
        }
        return sent_size;
    }

    int SocketUtil::connect_sock()
    {
        int ret = ::connect(m_sockfd, m_sockaddr_ptr, m_sockaddr_size);
        if (-1 == ret)
        {
            perror("connect sock failed");
            return -1;
        }
        return 0;
    }
} // namespace soda
