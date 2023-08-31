#pragma once

// UDP server/client

#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>
#include "socket_util.hpp"

namespace soda
{
    class UDPServer
    {
        // callback for /source, fd, addr, port, data, size
        using recv_cb_t = std::function<void(UDPServer &s,
                                             int32_t fd,
                                             const std::string &addr,
                                             uint16_t port,
                                             const void *data,
                                             size_t data_size)>;

    private:
        SocketUtil m_socket;
        int32_t m_sockfd;
        std::atomic_bool m_is_running;
        std::thread m_rcv_t;
        recv_cb_t m_callback_on_recv;

    public:
        UDPServer(const std::string &addr, uint16_t port);
        ~UDPServer();

        void set_callback_on_recv(recv_cb_t cb);

        // -1 if failed
        int start();

        void stop();

        void send(const void *src, size_t size, const std::string &addr, uint16_t port, int flags = 0);

    private:
        void recv();
    };

    void UDPServer::set_callback_on_recv(recv_cb_t cb)
    {
        m_callback_on_recv = std::move(cb);
    }

    UDPServer::UDPServer(const std::string &addr, uint16_t port) : m_socket(addr, port, SOCK_DGRAM, 0),
                                                                   m_sockfd(-1),
                                                                   m_is_running(false),
                                                                   m_rcv_t()
    {
    }

    UDPServer::~UDPServer()
    {
        stop();
        if (m_rcv_t.joinable())
        {
            m_rcv_t.join();
        }
    };

    void UDPServer::recv()
    {
        if (!m_callback_on_recv)
        {
            return;
        }

        AddrInfo ai{};
        uint8_t buf[4096];
        int ret = -1;
        while (m_is_running)
        {
            memset(buf, 0, sizeof(buf));
            ret = m_socket.recv_from(m_sockfd, buf, sizeof(buf), &ai);

            if (ret >= 0)
            {
                m_callback_on_recv(*this, m_sockfd, ai.addr, ai.port, buf, ret);
            }
            else
            {
                stop();
            }
        }
    }

    int UDPServer::start()
    {
        if (m_is_running)
        {
            return 0;
        }

        if (-1 == m_socket.start_udp_server())
        {
            return -1;
        }
        m_sockfd = m_socket.get_sockfd();
        m_is_running = true;

        m_rcv_t = std::move(std::thread(std::bind(&UDPServer::recv, this)));
        return 0;
    }

    void UDPServer::stop()
    {
        m_is_running = false;
    }

    void UDPServer::send(const void *src, size_t size, const std::string &addr, uint16_t port, int flags)
    {
        m_socket.send_to(src, size, addr, port, flags);
    }

} // namespace soda
