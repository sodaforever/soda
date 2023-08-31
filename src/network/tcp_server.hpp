#pragma once

// TCP server - multi-threaded; callback for conn/disconn/msg; the maximum clients online at the same time can be set, the default is 10

#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <unordered_map>
#include <tuple>
#include <functional>
#include <atomic>
#include <cstring>

#include "../thread/thread_pool.hpp"

namespace soda
{
    class TCPServer : Noncopyable
    {
        static const uint16_t DEFAULT_MAX_CONN = 10;

    public:
        // callback for conn /ip, port
        using conn_cb_t = std::function<void(const std::string &ip, uint16_t port)>;

        // callback for recv /fd, ip, port, data, size
        using recv_cb_t = std::function<void(uint32_t fd,
                                             const std::string &ip,
                                             uint16_t port,
                                             const void *data,
                                             size_t data_size)>;

        // callback for disconn /ip, port
        using disconn_cb_t = std::function<void(const std::string &ip, uint16_t port)>;

        TCPServer(uint16_t port, std::string ip = "::");
        ~TCPServer();

        void set_callback_on_conn(conn_cb_t cb);
        void set_callback_on_recv(recv_cb_t cb);
        void set_callback_on_disconn(disconn_cb_t cb);

        void start(size_t max_client_size = DEFAULT_MAX_CONN);

        void stop();

        void set_max_conn(size_t size);

        size_t get_conns() const;

        // -1 if failed
        int send(uint32_t fd, const void *src, size_t size);

        friend std::ostream &operator<<(std::ostream &os, const TCPServer &s)
        {
            return std::cout << "tcp_server -"
                             << " conn: " << s.m_conn_size
                             << "max: " << s.m_max_conn_size
                             << " running " << !s.m_is_stop
                             << std::endl;
        }

    private:
        std::string m_ip;
        uint16_t m_port;
        int32_t m_sockfd;
        sockaddr_in m_server_sockaddr;
        sockaddr_in m_client_sockaddr;
        struct Addr
        {
            std::string ip;
            uint16_t port;
        };
        std::unordered_map<uint16_t, Addr> m_conns;

        ThreadPool m_tp;

        std::atomic_bool m_is_stop;

        std::atomic_size_t m_max_conn_size;
        std::atomic_size_t m_conn_size;

        conn_cb_t m_callback_on_conn;
        recv_cb_t m_callback_on_recv;
        disconn_cb_t m_callback_on_disconn;

        std::mutex m_mtx;

    private:
        // -1 if failed
        int create_sockfd();

        // -1 if failed
        int create_sockaddr();

        // -1 if failed
        int bind_sock();

        // -1 if failed
        int listen_sock();

        void accept();

        void process_conn(uint32_t fd);

        void recv(uint32_t fd);

        void close_conn(uint32_t fd);
    };

    TCPServer::TCPServer(uint16_t port, std::string ip) : m_port(port),
                                                          m_ip(ip),
                                                          m_sockfd(-1),
                                                          m_is_stop(true),
                                                          m_tp(2, DEFAULT_MAX_CONN + 1),
                                                          m_max_conn_size(DEFAULT_MAX_CONN),
                                                          m_conn_size(0)
    {
        memset(&m_server_sockaddr, 0, sizeof(m_server_sockaddr));
        memset(&m_client_sockaddr, 0, sizeof(m_client_sockaddr));
    }

    TCPServer::~TCPServer()
    {
        stop();
    }

    void TCPServer::set_callback_on_conn(conn_cb_t cb)
    {
        m_callback_on_conn = std::move(cb);
    }
    void TCPServer::set_callback_on_recv(recv_cb_t cb)
    {
        m_callback_on_recv = std::move(cb);
    }
    void TCPServer::set_callback_on_disconn(disconn_cb_t cb)
    {
        m_callback_on_disconn = std::move(cb);
    }

    void TCPServer::start(size_t max_conn_size)
    {
        if (!m_is_stop)
        {
            return;
        }
        m_is_stop = false;
        set_max_conn(max_conn_size);

        int err = create_sockfd();
        err = create_sockaddr();
        int opt = 1;
        setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(m_sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

        err = bind_sock();
        err = listen_sock();

        if (-1 == err)
        {
            perror("tcp_server start failed");
            stop();
            return;
        }

        m_tp.insert_task_normal(std::bind(&TCPServer::accept, this));
    }

    void TCPServer::stop()
    {
        if (m_is_stop)
        {
            return;
        }
        m_is_stop = true;

        std::lock_guard<std::mutex> lock(m_mtx);

        for (auto &&conn : m_conns)
        {
            close(conn.first);
        }
        close(m_sockfd);
        m_conns.clear();
        m_tp.stop();
        m_conn_size = 0;
    }

    void TCPServer::set_max_conn(size_t size)
    {
        if (size < m_conn_size)
        {
            return;
        }

        // one more for accept
        m_tp.set_max_size(size + 1);
        m_max_conn_size = size;
    }

    int TCPServer::create_sockfd()
    {
        int sock_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (-1 == sock_fd)
        {
            perror("create socket fd failed");
            return -1;
        }
        return m_sockfd = sock_fd;
    }

    int TCPServer::create_sockaddr()
    {
        sockaddr_in &addr = m_server_sockaddr;
        addr.sin_family = AF_INET;
        if (-1 == inet_pton(AF_INET, m_ip.c_str(), &addr.sin_addr))
        {
            perror("ip addr invalid");
            return -1;
        }
        addr.sin_port = htons(m_port);
        return 0;
    }

    int TCPServer::bind_sock()
    {
        sockaddr *addr = reinterpret_cast<sockaddr *>(&m_server_sockaddr);
        socklen_t len = static_cast<socklen_t>(sizeof(sockaddr_in));
        if (-1 == ::bind(m_sockfd, addr, len))
        {
            perror("bind failed");
            return -1;
        }
        return 0;
    }

    int TCPServer::listen_sock()
    {
        if (-1 == ::listen(m_sockfd, SOMAXCONN))
        {
            perror("listen failed");
            return -1;
        }
        return 0;
    }

    void TCPServer::accept()
    {
        while (!m_is_stop && m_conn_size < m_max_conn_size)
        {
            sockaddr *addr = reinterpret_cast<sockaddr *>(&m_client_sockaddr);
            socklen_t len = static_cast<socklen_t>(sizeof(sockaddr_in));
            int32_t fd = ::accept(m_sockfd, addr, &len);

            if (-1 == fd)
            {
                perror("accept failed");
                stop();
                break;
            }

            ++m_conn_size;
            process_conn(fd);
        }
    }

    void TCPServer::close_conn(uint32_t fd)
    {
        std::lock_guard<std::mutex> lock(m_mtx);

        auto &conn = m_conns.find(fd)->second;

        if (m_callback_on_disconn)
        {
            m_callback_on_disconn(conn.ip, conn.port);
        }
        close(fd);
        m_conns.erase(fd);
        --m_conn_size;
    }

    void TCPServer::process_conn(uint32_t fd)
    {
        char ip_c[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &m_client_sockaddr.sin_addr, ip_c, sizeof(ip_c));
        uint16_t port = ntohs(m_client_sockaddr.sin_port);
        std::string ip{ip_c};
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_conns.emplace(fd, Addr{ip, port});
        }
        memset(&m_client_sockaddr, 0, sizeof(m_client_sockaddr));

        if (m_callback_on_conn)
        {
            m_callback_on_conn(ip, port);
        }
        m_tp.insert_task_normal(std::bind(&TCPServer::recv, this, fd));
    }

    void TCPServer::recv(uint32_t fd)
    {
        size_t buf_size = 1024;
        uint8_t buf[buf_size];

        while (!m_is_stop)
        {
            memset(buf, 0, buf_size);
            int ret = ::recv(fd, buf, buf_size, 0);
            if (-1 == ret && errno != EINTR)
            {
                perror("recv failed");
                close_conn(fd);
                break;
            }
            else if (0 == ret)
            {
                close_conn(fd);
                break;
            }
            else if (m_callback_on_recv)
            {
                std::string ip;
                uint16_t port;
                {
                    std::lock_guard<std::mutex> lock(m_mtx);
                    auto &conn = m_conns.find(fd)->second;
                    ip = conn.ip;
                    port = conn.port;
                }

                m_callback_on_recv(fd, ip, port, buf, ret);
            }
        }
    }

    size_t TCPServer::get_conns() const
    {
        return m_conn_size;
    }

    int TCPServer::send(uint32_t fd, const void *src, size_t size)
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_is_stop || m_conns.find(fd) == m_conns.end())
            {
                return 0;
            }
        }

        int ret = -1;
        do
        {
            ret = ::send(fd, src, size, 0);
        } while (-1 == ret && errno != EINTR);

        if (-1 == ret)
        {
            perror("send failed");
            close_conn(fd);
            return ret;
        }
        else if (ret < size)
        {
            return ret += send(fd, reinterpret_cast<const uint8_t *>(src) + ret, size - ret);
        }
        else if (ret == size)
        {
            return ret;
        }
        return size;
    }
} // namespace TCPServer
