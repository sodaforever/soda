#pragma once

// TCP server - poll version; multi-threaded event processing; callback processes conn, msg, disconn; non-blocking IO; IPv4
// ! ! ! Message callback needs to ensure multi-thread safety

#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <iostream>
#include <unordered_map>
#include <tuple>
#include <functional>
#include <atomic>
#include <cstring>

#include "../thread/thread_pool.hpp"

namespace soda
{
    class SelectTCPServer : Noncopyable
    {
        // Default maximum number of connections
        static const uint16_t DEFAULT_SELECT_MAX_CONN = 1000;

    public:
        // callback for conn /source, addr, port
        using conn_cb_t = std::function<void(SelectTCPServer &s, int32_t fd, const std::string &addr, uint16_t port)>;

        // callback for recv msg /source, fd, addr, port, data, size
        using recv_cb_t = std::function<void(SelectTCPServer &s,
                                             int32_t fd,
                                             const std::string &addr,
                                             uint16_t port,
                                             const void *data,
                                             size_t data_size)>;

        // callback for disconn /source, addr, port
        using disconn_cb_t = std::function<void(SelectTCPServer &s, const std::string &addr, uint16_t port)>;

        SelectTCPServer(uint16_t port, std::string ip = "::");
        ~SelectTCPServer();

        void set_callback_on_conn(conn_cb_t cb);
        void set_callback_on_recv(recv_cb_t cb);
        void set_callback_on_disconn(disconn_cb_t cb);

        void start(size_t max_client_size = DEFAULT_SELECT_MAX_CONN);

        void stop();

        void close(uint32_t fd);

        void set_max_conn(size_t size);

        size_t get_conns() const;

        // -1 if failed
        int send(uint32_t fd, const void *data, size_t size);

        friend std::ostream &operator<<(std::ostream &os, const SelectTCPServer &s)
        {
            return os << "tcp_server -"
                      << " conn: " << s.m_conn_size
                      << " max: " << s.m_max_conn_size
                      << " running " << !s.m_stop
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

        std::atomic_bool m_stop;

        std::atomic_size_t m_max_conn_size;
        std::atomic_size_t m_conn_size;

        conn_cb_t m_callback_on_conn;
        recv_cb_t m_callback_on_recv;
        disconn_cb_t m_callback_on_disconn;

        std::mutex m_mtx;

        fd_set m_fds;

        std::atomic_uint16_t m_max_fd;

        // wakeup fd
        int m_pipe[2];

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

        void select_start();
    };

    SelectTCPServer::SelectTCPServer(uint16_t port, std::string ip) : m_ip(ip),
                                                                      m_port(port),
                                                                      m_sockfd(-1),
                                                                      m_tp(2),
                                                                      m_stop(true),
                                                                      m_max_conn_size(DEFAULT_SELECT_MAX_CONN),
                                                                      m_conn_size(0),
                                                                      m_max_fd(0)
    {
        memset(&m_server_sockaddr, 0, sizeof(m_server_sockaddr));
        memset(&m_client_sockaddr, 0, sizeof(m_client_sockaddr));
    }

    SelectTCPServer::~SelectTCPServer()
    {
        stop();
    }

    void SelectTCPServer::set_callback_on_conn(conn_cb_t cb)
    {
        m_callback_on_conn = std::move(cb);
    }
    void SelectTCPServer::set_callback_on_recv(recv_cb_t cb)
    {
        m_callback_on_recv = std::move(cb);
    }
    void SelectTCPServer::set_callback_on_disconn(disconn_cb_t cb)
    {
        m_callback_on_disconn = std::move(cb);
    }

    void SelectTCPServer::start(size_t max_conn_size)
    {
        if (!m_stop)
        {
            return;
        }
        m_stop = false;
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
        m_tp.insert_task_normal(std::bind(&SelectTCPServer::select_start, this));
    }

    void SelectTCPServer::select_start()
    {
        // 初始化pipe
        if (-1 == pipe(m_pipe))
        {
            perror("pipe init failed");
            return;
        }

        FD_ZERO(&m_fds);
        FD_SET(m_sockfd, &m_fds);
        FD_SET(m_pipe[0], &m_fds);
        m_max_fd = m_pipe[0];
        fd_set fds;
        FD_ZERO(&fds);

#ifndef FD_COPY
#define FD_COPY(fd, fd2) memcpy((fd2), (fd), sizeof(*(fd)))
#endif

        while (!m_stop)
        {
            FD_COPY(&m_fds, &fds);

            int act_size = select(m_max_fd + 1, &fds, nullptr, nullptr, nullptr);
            if (-1 == act_size)
            {
                perror("select error");
                stop();
                break;
            }

            // completed recv
            if (FD_ISSET(m_pipe[0], &fds))
            {
                uint32_t fd = 0;
                if (-1 == read(m_pipe[0], &fd, sizeof(fd)))
                {
                    perror("poll error");
                    stop();
                    break;
                }
                continue;
            }

            //  accept
            if (FD_ISSET(m_sockfd, &fds) && m_conn_size < m_max_conn_size)
            {
                accept();
            }

            // recv
            std::lock_guard<std::mutex> lock(m_mtx);
            for (auto &&conn : m_conns)
            {
                if (FD_ISSET(conn.first, &fds))
                {
                    FD_CLR(conn.first, &m_fds);
                    m_tp.insert_task_normal(std::bind(&SelectTCPServer::recv, this, conn.first));
                }
            }
        }
    }

    void SelectTCPServer::stop()
    {
        if (m_stop)
        {
            return;
        }
        m_stop = true;

        for (auto &&conn : m_conns)
        {
            close(conn.first);
        }

        std::lock_guard<std::mutex> lock(m_mtx);

        FD_ZERO(&m_fds);
        ::close(m_sockfd);
        ::close(m_pipe[0]);
        ::close(m_pipe[1]);
        m_conns.clear();
        m_tp.stop();
        m_conn_size = 0;
    }

    void SelectTCPServer::set_max_conn(size_t size)
    {
        if (size < m_conn_size)
        {
            return;
        }

        // one more for accept
        m_tp.set_max_size(size + 1);
        m_max_conn_size = size;
    }

    // -1 if failed
    int SelectTCPServer::create_sockfd()
    {
        int sock_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (-1 == sock_fd)
        {
            perror("create socket fd failed");
            return -1;
        }
        return m_sockfd = sock_fd;
    }

    // -1 if failed
    int SelectTCPServer::create_sockaddr()
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

    // -1 if failed
    int SelectTCPServer::bind_sock()
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

    // -1 if failed
    int SelectTCPServer::listen_sock()
    {
        if (-1 == ::listen(m_sockfd, SOMAXCONN))
        {
            perror("listen failed");
            return -1;
        }
        return 0;
    }

    void SelectTCPServer::accept()
    {
        if (m_conn_size < m_max_conn_size)
        {
            sockaddr *addr = reinterpret_cast<sockaddr *>(&m_client_sockaddr);
            socklen_t len = static_cast<socklen_t>(sizeof(sockaddr_in));
            int32_t fd = ::accept(m_sockfd, addr, &len);

            if (-1 == fd)
            {
                perror("accept failed");
                stop();
            }
            else
            {
                process_conn(fd);
            }
        }
    }

    void SelectTCPServer::close(uint32_t fd)
    {
        std::lock_guard<std::mutex> lock(m_mtx);

        auto &conn = m_conns.find(fd)->second;

        if (m_callback_on_disconn)
        {
            m_callback_on_disconn(*this, conn.ip, conn.port);
        }
        FD_CLR(fd, &m_fds);
        if (m_max_fd == fd)
        {
            for (size_t i = fd; i > 0; --i)
            {
                if (FD_ISSET(i, &m_fds))
                {
                    m_max_fd = i;
                    break;
                }
            }
        }
        ::close(fd);
        m_conns.erase(fd);
        --m_conn_size;
    }

    void SelectTCPServer::process_conn(uint32_t fd)
    {
        char ip_c[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &m_client_sockaddr.sin_addr, ip_c, sizeof(ip_c));
        uint16_t port = ntohs(m_client_sockaddr.sin_port);
        std::string ip{ip_c};
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_conns.emplace(fd, Addr{ip, port});
            FD_SET(fd, &m_fds);
            if (fd > m_max_fd)
            {
                m_max_fd = fd;
            }
        }
        ++m_conn_size;
        memset(&m_client_sockaddr, 0, sizeof(m_client_sockaddr));

        if (m_callback_on_conn)
        {
            m_callback_on_conn(*this, fd, ip, port);
        }
    }

    void SelectTCPServer::recv(uint32_t fd)
    {
        constexpr size_t buf_size = 4096;
        uint8_t buf[buf_size];

        memset(buf, 0, buf_size);
        int ret = ::recv(fd, buf, buf_size, 0);
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            FD_SET(fd, &m_fds);
            write(m_pipe[1], &fd, sizeof(fd));
        }

        if (-1 == ret && errno != EINTR)
        {
            perror("recv failed");
            close(fd);
        }
        else if (0 == ret)
        {
            close(fd);
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
            m_callback_on_recv(*this, fd, ip, port, buf, ret);
        }
    }

    size_t SelectTCPServer::get_conns() const
    {
        return m_conn_size;
    }

    // -1 if failed
    int SelectTCPServer::send(uint32_t fd, const void *src, size_t size)
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_stop || m_conns.find(fd) == m_conns.end())
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
            close(fd);
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
} // namespace SelectTCPServer
