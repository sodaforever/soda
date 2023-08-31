#pragma once

// epoll TCP server - multi-threading event processing; callback processes conn, msg, disconn; non-blocking IO; IPv4/IPv6

#include <unordered_map>
#include <mutex>

#include "socket_util.hpp"
#include "epoller.hpp"
#include "../thread/thread_pool.hpp"

namespace soda
{
    class EpollTCPServer
    {
        // callback for conn /source, addr, port
        using conn_cb_t = std::function<void(EpollTCPServer &s, int32_t fd, const std::string &addr, uint16_t port)>;

        // callback for recv msg /source, fd, addr, port, data, size
        using recv_cb_t = std::function<void(EpollTCPServer &s,
                                             int32_t fd,
                                             const std::string &addr,
                                             uint16_t port,
                                             const void *data,
                                             size_t data_size)>;

        // callback for disconn /source, addr, port
        using disconn_cb_t = std::function<void(EpollTCPServer &s, const std::string &addr, uint16_t port)>;

        using conn_info_ptr = SocketUtil::conn_info_ptr;

    private:
        SocketUtil m_socket;
        int32_t m_sockfd;
        ThreadPool m_tp;
        Epoller m_epoller;
        std::unordered_map<int32_t, conn_info_ptr> m_conns;
        std::mutex m_mtx;
        conn_cb_t m_callback_on_conn;
        recv_cb_t m_callback_on_recv;
        disconn_cb_t m_callback_on_disconn;

        std::atomic_bool m_stop;

    public:
        EpollTCPServer(const std::string &addr, uint16_t port);
        ~EpollTCPServer();

        void set_callback_on_conn(conn_cb_t cb);
        void set_callback_on_recv(recv_cb_t cb);
        void set_callback_on_disconn(disconn_cb_t cb);

        // start service
        // return -1 on failure
        int start();

        void stop();

        // -1 if failed; the amount of data sent, and will retry to send all the data
        int send(uint32_t fd, const void *src, size_t size, int flags = 0);

        // -1 if failed; the amount of data sent, and will retry to send all the data
        int sendfile(uint32_t dstfd, uint32_t srcfd, off_t *offset, size_t size);

        // send message to all clients
        void send_to_all(const void *src, size_t size, int flags = 0);

        friend std::ostream &operator<<(std::ostream &os, const EpollTCPServer &s)
        {
            return os << "clients: " << s.m_conns.size()
                      << " running " << !s.m_stop
                      << std::endl;
        }

        void close(int32_t fd);

    private:
        // -1 if failed
        int listen();
        void accept();
        void recv(int32_t fd);

        // nullptr if not exists
        conn_info_ptr get_conn(int32_t fd);
    };

    EpollTCPServer::conn_info_ptr EpollTCPServer::get_conn(int32_t fd)
    {
        std::lock_guard<std::mutex> lock(m_mtx);

        auto conn_iter = m_conns.find(fd);
        if (conn_iter == m_conns.end())
        {
            return nullptr;
        }
        return conn_iter->second;
    }

    EpollTCPServer::EpollTCPServer(const std::string &addr, uint16_t port) : m_socket(addr, port, SOCK_STREAM, 0),
                                                                             m_sockfd(-1),
                                                                             m_tp(2),
                                                                             m_stop(true) {}

    EpollTCPServer::~EpollTCPServer()
    {
        stop();
    }

    int EpollTCPServer::listen()
    {
        while (!m_stop)
        {
            auto &&ret = m_epoller.check_once();
            int size = std::get<0>(ret);
            if (size > 0)
            {
                auto &&events = std::get<1>(ret).get();
                for (int i = 0; i < size; ++i)
                {
                    auto &&ev = events[i];
                    int32_t fd = ev.data.fd;
                    if (m_sockfd == fd)
                    {
                        m_tp.insert_task_normal(std::bind(&EpollTCPServer::accept, this));
                    }
                    else if (ev.events & EPOLLERR)
                    {
                        m_tp.insert_task_normal(std::bind(&EpollTCPServer::close, this, fd));
                    }
                    else if (ev.data.fd > 0)
                    {
                        m_tp.insert_task_normal(std::bind(&EpollTCPServer::recv, this, fd));
                    }
                }
            }
        }
    }

    void EpollTCPServer::accept()
    {
        while (!m_stop)
        {
            conn_info_ptr conn = m_socket.accept();
            if (!conn)
            {
                stop();
                break;
            }
            else if (-1 == conn->fd)
            {
                break;
            }
            // NIO
            m_socket.set_nonblocking(conn->fd);
            std::lock_guard<std::mutex> lock(m_mtx);
            m_conns.emplace(conn->fd, conn);
            // Join the listening queue, edge trigger, use oneshot to avoid single descriptor multi-thread competition
            m_epoller.add_event(conn->fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
            if (m_callback_on_conn)
            {
                m_callback_on_conn(*this, conn->fd, conn->addr, conn->port);
            }
        }
        // reactivate
        m_epoller.mod_event(m_sockfd, EPOLLIN | EPOLLET | EPOLLONESHOT);
    }

    void EpollTCPServer::recv(int fd)
    {
        if (!m_callback_on_recv)
        {
            return;
        }

        conn_info_ptr conn = get_conn(fd);
        if (!conn)
        {
            return;
        }

        uint8_t buf[4096];
        int ret = -1;
        // NIO, read multiple times
        while (!m_stop)
        {
            memset(buf, 0, sizeof(buf));
            ret = m_socket.recv(fd, buf, sizeof(buf));
            // if buffer is full, may need to read again
            if (ret > 0)
            {
                m_callback_on_recv(*this, fd, conn->addr, conn->port, buf, ret);

                if (sizeof(buf) == ret)
                {
                    continue;
                }
            }
            else if (-1 == ret)
            {
                close(fd);
                return;
            }
            break;
        }
        // reactivate
        m_epoller.mod_event(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
    }

    void EpollTCPServer::stop()
    {
        if (m_stop)
        {
            return;
        }
        m_stop = true;

        for (auto iter = m_conns.begin(); iter != m_conns.end();)
        {
            close(iter->first);
            iter = m_conns.begin();
        }
        std::lock_guard<std::mutex> lock(m_mtx);
        m_conns.clear();
        m_epoller.stop();
        m_tp.stop();
        m_socket.stop();
    }

    void EpollTCPServer::close(int fd)
    {
        conn_info_ptr conn = get_conn(fd);
        if (!conn)
        {
            return;
        }

        if (m_callback_on_disconn)
        {
            m_callback_on_disconn(*this, conn->addr, conn->port);
        }
        std::lock_guard<std::mutex> lock(m_mtx);
        m_conns.erase(fd);
        m_epoller.del_event(fd);
        m_socket.close_conn(fd);
    }

    // -1 if failed
    int EpollTCPServer::start()
    {
        if (!m_stop)
        {
            return 0;
        }
        m_stop = false;

        if (-1 == m_socket.start_tcp_server())
        {
            return -1;
        }
        m_sockfd = m_socket.get_sockfd();
        // NIO
        m_socket.set_nonblocking(m_sockfd);
        // edge trigger
        m_epoller.start();
        m_epoller.add_event(m_sockfd, EPOLLIN | EPOLLET | EPOLLONESHOT);
        m_tp.start();
        m_tp.insert_task_normal(std::bind(&EpollTCPServer::listen, this));
        return 0;
    }

    int EpollTCPServer::send(uint32_t fd, const void *src, size_t size, int flags)
    {
        if (!get_conn(fd))
        {
            return -1;
        }

        int ret = m_socket.send(fd, src, size, flags);
        if (-1 == ret)
        {
            close(fd);
        }
        return ret;
    }

    int EpollTCPServer::sendfile(uint32_t dstfd, uint32_t srcfd, off_t *offset, size_t size)
    {
        if (!get_conn(dstfd))
        {
            return -1;
        }

        int ret = m_socket.sendfile(dstfd, srcfd, offset, size);
        if (-1 == ret)
        {
            DEBUG_PRINT("sendfile failed");
            close(dstfd);
            return -1;
        }
        return ret;
    }

    void EpollTCPServer::send_to_all(const void *src, size_t size, int flags)
    {
        for (auto &&ele : m_conns)
        {
            send(ele.first, src, size, flags);
        }
    }

    void EpollTCPServer::set_callback_on_conn(conn_cb_t cb)
    {
        m_callback_on_conn = std::move(cb);
    }
    void EpollTCPServer::set_callback_on_recv(recv_cb_t cb)
    {
        m_callback_on_recv = std::move(cb);
    }
    void EpollTCPServer::set_callback_on_disconn(disconn_cb_t cb)
    {
        m_callback_on_disconn = std::move(cb);
    }

} // namespace soda
