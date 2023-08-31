#pragma once

// TCP server - poll version; multi-threaded event processing; callback processes conn, msg, disconn; non-blocking IO; IPv4

#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <iostream>
#include <unordered_map>
#include <tuple>
#include <functional>
#include <atomic>
#include <vector>
#include <fcntl.h>
#include <cstring>

#include "../thread/thread_pool.hpp"

namespace soda
{
    class PollTCPServer : Noncopyable
    {
        // default maximum number of connections
        static const uint16_t DEFAULT_POLL_MAX_CONN = 1000;

    public:
        // callback for conn /source, addr, port
        using conn_cb_t = std::function<void(PollTCPServer &s, int32_t fd, const std::string &addr, uint16_t port)>;

        // callback for recv msg /source, fd, addr, port, data, size
        using recv_cb_t = std::function<void(PollTCPServer &s,
                                             int32_t fd,
                                             const std::string &addr,
                                             uint16_t port,
                                             const void *data,
                                             size_t data_size)>;

        // callback for disconn /source, addr, port
        using disconn_cb_t = std::function<void(PollTCPServer &s, const std::string &addr, uint16_t port)>;

        PollTCPServer(uint16_t port, std::string ip = "::");
        ~PollTCPServer();

        void set_callback_on_conn(conn_cb_t cb);
        void set_callback_on_recv(recv_cb_t cb);
        void set_callback_on_disconn(disconn_cb_t cb);

        void start(size_t max_client_size = DEFAULT_POLL_MAX_CONN);

        void stop();

        void close(uint32_t fd);

        void set_max_conn(size_t size);

        size_t get_conns() const;

        // -1 if failed
        int send(uint32_t fd, const void *src, size_t size);

        friend std::ostream &operator<<(std::ostream &os, const PollTCPServer &s)
        {
            return os << "tcp_server -"
                      << " conn: " << s.m_conn_size
                      << " max: " << s.m_max_conn_size
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

        std::vector<pollfd> m_fds;

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

        void poll_start();

        void set_pollfd(uint32_t fd, uint16_t events);

        void del_pollfd(uint32_t fd);

        int set_nonblocking(uint32_t fd);
    };

    PollTCPServer::PollTCPServer(uint16_t port, std::string ip) : m_ip(ip),
                                                                  m_port(port),
                                                                  m_sockfd(-1),
                                                                  m_tp(2, DEFAULT_POLL_MAX_CONN + 1),
                                                                  m_is_stop(true),
                                                                  m_max_conn_size(DEFAULT_POLL_MAX_CONN),
                                                                  m_conn_size(0)

    {
        memset(&m_server_sockaddr, 0, sizeof(m_server_sockaddr));
        memset(&m_client_sockaddr, 0, sizeof(m_client_sockaddr));
    }

    PollTCPServer::~PollTCPServer()
    {
        stop();
    }

    void PollTCPServer::set_callback_on_conn(conn_cb_t cb)
    {
        m_callback_on_conn = std::move(cb);
    }
    void PollTCPServer::set_callback_on_recv(recv_cb_t cb)
    {
        m_callback_on_recv = std::move(cb);
    }
    void PollTCPServer::set_callback_on_disconn(disconn_cb_t cb)
    {
        m_callback_on_disconn = std::move(cb);
    }

    void PollTCPServer::start(size_t max_conn_size)
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
        m_tp.insert_task_normal(std::bind(&PollTCPServer::poll_start, this));
    }

    void PollTCPServer::poll_start()
    {
        if (-1 == pipe(m_pipe))
        {
            perror("pipe init failed");
            return;
        }

        set_pollfd(m_sockfd, POLLIN);
        set_pollfd(m_pipe[0], POLLIN);

        set_nonblocking(m_pipe[0]);

        while (!m_is_stop)
        {
            int act_size = poll(m_fds.data(), m_fds.size(), -1);

            if (-1 == act_size)
            {
                perror("poll error");
                stop();
                break;
            }

            // completed recv
            if (m_fds[1].revents & POLLIN)
            {
                uint32_t fd = 0;
                while (true)
                {
                    int ret = read(m_pipe[0], &fd, sizeof(fd));
                    if (-1 == ret)
                    {
                        if (EWOULDBLOCK == errno || EAGAIN == errno)
                        {
                            break;
                        }
                        else if (EINTR == errno)
                        {
                            continue;
                        }
                        else
                        {
                            perror("pipe recv failed");
                            stop();
                            break;
                        }
                    }
                    else if (0 == ret)
                    {
                        ERROR_PRINT("pipe recv closed");
                        stop();
                        break;
                    }

                    std::lock_guard<std::mutex> lock(m_mtx);
                    set_pollfd(fd, POLLIN);
                }
            }

            // accept
            if (m_fds[0].revents & POLLIN && m_conn_size < m_max_conn_size)
            {
                accept();
            }

            // recv
            std::lock_guard<std::mutex> lock(m_mtx);
            for (size_t i = 2; i < m_fds.size(); ++i)
            {
                if (1 == (m_fds[i].events & m_fds[i].revents & POLLIN))
                {
                    m_fds[i].events = 0;
                    m_fds[i].revents = 0;
                    m_tp.insert_task_normal(std::bind(&PollTCPServer::recv, this, m_fds[i].fd));
                }
            }
        }
    }

    void PollTCPServer::stop()
    {
        if (m_is_stop)
        {
            return;
        }
        m_is_stop = true;

        for (auto &&conn : m_conns)
        {
            close(conn.first);
        }

        std::lock_guard<std::mutex> lock(m_mtx);
        m_fds.clear();
        ::close(m_sockfd);
        ::close(m_pipe[0]);
        ::close(m_pipe[1]);
        m_conns.clear();
        m_tp.stop();
        m_conn_size = 0;
    }

    void PollTCPServer::set_max_conn(size_t size)
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
    int PollTCPServer::create_sockfd()
    {
        m_sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (-1 == m_sockfd)
        {
            perror("create socket fd failed");
            return -1;
        }
        return m_sockfd;
    }

    // -1 if failed
    int PollTCPServer::create_sockaddr()
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
    int PollTCPServer::bind_sock()
    {
        sockaddr *addr = reinterpret_cast<sockaddr *>(&m_server_sockaddr);
        socklen_t len = static_cast<socklen_t>(sizeof(sockaddr));
        if (-1 == ::bind(m_sockfd, addr, len))
        {
            perror("bind failed");
            return -1;
        }
        return 0;
    }

    // -1 if failed
    int PollTCPServer::listen_sock()
    {
        if (-1 == ::listen(m_sockfd, SOMAXCONN))
        {
            perror("listen failed");
            return -1;
        }
        return 0;
    }

    void PollTCPServer::accept()
    {
        if (m_conn_size < m_max_conn_size)
        {
            sockaddr *addr = reinterpret_cast<sockaddr *>(&m_client_sockaddr);
            socklen_t len = static_cast<socklen_t>(sizeof(sockaddr));
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

    void PollTCPServer::close(uint32_t fd)
    {
        std::lock_guard<std::mutex> lock(m_mtx);

        auto &conn = m_conns.find(fd)->second;

        if (m_callback_on_disconn)
        {
            m_callback_on_disconn(*this, conn.ip, conn.port);
        }
        del_pollfd(fd);
        ::close(fd);
        m_conns.erase(fd);
        --m_conn_size;
    }

    void PollTCPServer::process_conn(uint32_t fd)
    {
        set_nonblocking(fd);

        char ip_c[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &m_client_sockaddr.sin_addr, ip_c, sizeof(ip_c));
        uint16_t port = ntohs(m_client_sockaddr.sin_port);
        std::string ip{ip_c};
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_conns.emplace(fd, Addr{ip, port});
            set_pollfd(fd, POLLIN);
        }

        ++m_conn_size;
        memset(&m_client_sockaddr, 0, sizeof(m_client_sockaddr));

        if (m_callback_on_conn)
        {
            m_callback_on_conn(*this, fd, ip, port);
        }
    }

    void PollTCPServer::recv(uint32_t fd)
    {
        size_t buf_size = 4096;
        uint8_t buf[buf_size];

        while (true)
        {
            memset(buf, 0, buf_size);
            int ret = ::recv(fd, buf, buf_size, 0);

            if (ret > 0 && m_callback_on_recv)
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
            else if (0 == ret)
            {
                close(fd);
                break;
            }
            else
            {
                if (EINTR == errno)
                {
                    continue;
                }
                else if (EAGAIN == errno || EWOULDBLOCK == errno)
                {
                    std::lock_guard<std::mutex> lock(m_mtx);
                    // wake poll up
                    write(m_pipe[1], &fd, sizeof(fd));
                    break;
                }
                else
                {
                    perror("recv failed");
                    close(fd);
                    break;
                }
            }
        }
    }

    size_t PollTCPServer::get_conns() const
    {
        return m_conn_size;
    }

    int PollTCPServer::send(uint32_t fd, const void *src, size_t size)
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

    void PollTCPServer::set_pollfd(uint32_t fd, uint16_t events)
    {
        for (size_t i = 0; i < m_fds.size(); i++)
        {
            if (m_fds[i].fd == fd)
            {
                m_fds[i].events = events;
                return;
            }
        }
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = events;
        m_fds.push_back(std::move(pfd));
    }

    void PollTCPServer::del_pollfd(uint32_t fd)
    {
        for (size_t i = 0; i < m_fds.size(); i++)
        {
            if (m_fds[i].fd == fd)
            {
                m_fds.erase(m_fds.begin() + i);
                return;
            }
        }
    }

    int PollTCPServer::set_nonblocking(uint32_t fd)
    {
        int flags;

        flags = fcntl(fd, F_GETFL, 0);
        if (-1 == flags)
        {
            perror("get fd flags failed");
            return -1;
        }

        flags |= O_NONBLOCK;

        if (-1 == fcntl(fd, F_SETFL, flags))
        {
            perror("set fd flags failed");
            return -1;
        }

        return 0; // Success
    }
} // namespace PollTCPServer
