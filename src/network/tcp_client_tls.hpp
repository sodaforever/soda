#pragma once

// tcp client - tls version; callback for conn, msg, disconn; multi-thread processing for recv; automatic reconnection

#include <functional>
#include <atomic>

#include "socket_util.hpp"
#include "../thread/simple_thread_pool.hpp"
#include "../general/random.hpp"
#include "tls_util.hpp"

namespace soda
{
    class TCPClient
    {
        // callback for conn /source, ip, port
        using conn_cb_t = std::function<void(TCPClient &c, std::string &addr, uint16_t port)>;

        // callback for recv /source, fd, ip, port, data, size
        using recv_cb_t = std::function<void(TCPClient &c,
                                             int32_t fd,
                                             const std::string &addr,
                                             uint16_t port,
                                             const void *data,
                                             size_t data_size)>;

        // callback for disconn /source, ip, port
        using disconn_cb_t = std::function<void(TCPClient &c, const std::string &addr, uint16_t port)>;

    private:
        SocketUtil m_socket;
        int32_t m_sockfd;
        std::string m_addr;
        uint16_t m_port;

        std::atomic_bool m_connected;
        bool m_need_reconn;
        size_t m_reconn_interval;
        int32_t m_reconn_times;

        std::thread m_rcv_t;

        conn_cb_t m_callback_on_conn;
        recv_cb_t m_callback_on_recv;
        disconn_cb_t m_callback_on_disconn;

        TLSUtil m_tls;
        TLSUtil::ssl_ptr m_ssl;
        std::atomic_bool m_ssl_connected;

    public:
        TCPClient(const std::string &addr, uint16_t port);
        ~TCPClient();

        void set_callback_on_conn(conn_cb_t cb);
        void set_callback_on_recv(recv_cb_t cb);
        void set_callback_on_disconn(disconn_cb_t cb);

        bool set_crt_key(const std::string &crt, const std::string &key, int file_type = SSL_FILETYPE_PEM) const;
        bool set_CA(const std::string &cert) const;
        void set_if_verify_peer_crt(bool verify) const;

        void start();

        void stop();

        // -1 if failed
        int send(const void *src, size_t size);

        void set_reconn(bool enable, int interval, int times);

    private:
        void recv();
        void close();
        // -1 if failed
        int connect();
        // -1 if failed
        int reconnect();
        // -1 if failed
        int tls_connect();

        // -1 if disconnected
        inline int check_connection() const;
    };

    TCPClient::TCPClient(const std::string &addr, uint16_t port) : m_socket(addr, port, SOCK_STREAM, 0),
                                                                   m_sockfd(-1),
                                                                   m_connected(false),
                                                                   m_need_reconn(true),
                                                                   m_reconn_interval(5000 + random::get_int(-2000, 2000)),
                                                                   m_reconn_times(20),
                                                                   m_tls(false),
                                                                   m_ssl(nullptr),
                                                                   m_ssl_connected(false)

    {
    }

    TCPClient::~TCPClient()
    {
        m_socket.close_sockfd(m_sockfd);
        if (m_rcv_t.joinable())
        {
            m_rcv_t.join();
        }
    }

    void TCPClient::set_if_verify_peer_crt(bool verify) const
    {
        m_tls.set_if_verify_peer_crt(verify);
    }

    bool TCPClient::set_CA(const std::string &crt) const
    {
        return m_tls.set_CA(crt);
    }

    bool TCPClient::set_crt_key(const std::string &crt, const std::string &key, int file_type) const
    {
        return m_tls.set_crt_key(crt, key, file_type);
    }

    inline int TCPClient::check_connection() const
    {
        if (!m_connected || !m_ssl_connected || !m_ssl)
        {
            return -1;
        }
        return 0;
    }

    void TCPClient::start()
    {
        if (-1 != connect())
        {
            m_rcv_t = std::move(std::thread(std::bind(&TCPClient::recv, this)));
        }
        else
        {
            stop();
        }
    }

    void TCPClient::stop()
    {
        close();
        if (m_rcv_t.joinable())
        {
            m_rcv_t.join();
        }
    }

    int TCPClient::tls_connect()
    {
        m_ssl = m_tls.get_ssl(m_sockfd);
        if (!m_ssl)
        {
            return -1;
        }
        if (1 != m_tls.connect(m_ssl))
        {
            return -1;
        }

        m_ssl_connected = true;
        return 0;
    }

    int TCPClient::connect()
    {
        if (-1 == m_socket.start_tcp_client())
        {
            return -1;
        }
        m_connected = true;
        m_sockfd = m_socket.get_sockfd();
        m_addr = m_socket.get_addr();
        m_port = m_socket.get_port();

        if (m_callback_on_conn)
        {
            m_callback_on_conn(*this, m_addr, m_port);
        }
        return tls_connect();
    }

    int TCPClient::reconnect()
    {
        if (-1 != check_connection() || !m_need_reconn)
        {
            return 0;
        }

        m_socket.close_sockfd(m_sockfd);

        // if m_reconn_times==-1，decrease from MAX_SIZE_T
        for (size_t i = m_reconn_times; i > 0; --i)
        {
            if (-1 != connect())
            {
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(m_reconn_interval));
        }
        return -1;
    }

    // -1 if failed
    int TCPClient::send(const void *src, size_t size)
    {
        if (-1 == check_connection())
        {
            return -1;
        }

        int ret = m_tls.send(m_ssl, src, size);
        if (ret >= 0)
        {
            return ret;
        }
        else if (-1 == ret)
        {
            close();
            reconnect();
        }
        return -1;
    }

    void TCPClient::recv()
    {
        if (-1 == check_connection())
        {
            return;
        }

        uint8_t buf[4096];
        int ret = -1;
        // NIO
        while (m_connected)
        {
            memset(buf, 0, sizeof(buf));
            ret = m_tls.recv(m_ssl, buf, sizeof(buf));
            if (ret > 0)
            {
                if (m_callback_on_recv)
                {
                    m_callback_on_recv(*this, m_sockfd, m_addr, m_port, buf, ret);
                }
            }
            else if (ret <= 0)
            {
                close();
                reconnect();
            }
        }
    }

    void TCPClient::close()
    {
        if (!m_connected)
        {
            return;
        }

        m_connected = false;
        m_ssl_connected = false;

        if (m_callback_on_disconn)
        {
            m_callback_on_disconn(*this, m_addr, m_port);
        }
        if (m_ssl)
        {
            m_ssl.reset();
        }

        m_socket.close_sockfd(m_sockfd);
    }

    void TCPClient::set_reconn(bool enable, int interval, int times)
    {
        m_need_reconn = enable;
        m_reconn_interval = interval + random::get_int(-(interval / 2), 2000);
        m_reconn_times = times;
    }

    void TCPClient::set_callback_on_conn(conn_cb_t cb)
    {
        m_callback_on_conn = std::move(cb);
    }
    void TCPClient::set_callback_on_recv(recv_cb_t cb)
    {
        m_callback_on_recv = std::move(cb);
    }
    void TCPClient::set_callback_on_disconn(disconn_cb_t cb)
    {
        m_callback_on_disconn = std::move(cb);
    }

} // namespace soda
