#pragma once

// connection poll - contain db-conn which inherits from ConnBase
// automatically scale within min and max size; reconnection;

#include <memory>
#include <atomic>
#include <thread>
#include <functional>

#include "../general/util.hpp"
#include "conn_base.hpp"
#include "../queue/blocking_queue.hpp"
#include "../thread/atomic_unordered_set.hpp"

namespace soda
{
    template <typename T>
    class ConnPool : Noncopyable
    {

        using LimitedType = enable_if_t<std::is_base_of<ConnBase, T>::value>;
        // ms
        static const size_t MAX_IDLE_DURATION_TO_CLOSE_CONN = 300000;
        static const size_t MONITOR_SLEEP_TIME = 30000;

    private:
        std::string m_conn_str;

        size_t m_max_size;
        size_t m_min_size;

        std::atomic_size_t m_conn_size;
        std::atomic_size_t m_waiting_size;

        std::atomic_bool m_stop;

        std::thread m_monitor;

        using conn_ptr = std::shared_ptr<T>;
        BlockingQueue<conn_ptr> m_idle_conns;
        AtomicUnorderedSet<conn_ptr> m_busy_conns;

        std::mutex m_mtx;

    public:
        ConnPool(const std::string &conn_str, size_t min_size = 1, size_t max_size = std::thread::hardware_concurrency());
        ~ConnPool();

        conn_ptr acquire();
        void release(conn_ptr &&conn);

        size_t size() const;
        size_t busy_size() const;
        void set_min_size(size_t size);
        void set_max_size(size_t size);

        friend std::ostream &operator<<(std::ostream &os, const ConnPool<T> &cp)
        {
            return os << "conn_poll -"
                      << " all: " << cp.m_conn_size
                      << " idle: " << cp.m_idle_conns.size()
                      << " waiting: " << cp.m_waiting_size
                      << " min: " << cp.m_min_size
                      << " max: " << cp.m_max_size
                      << std::endl;
        }

    private:
        void init();
        void add_conn();

        void monitor();
        void check_scale();
        void check_connection();
    };

    template <typename T>
    size_t ConnPool<T>::size() const
    {
        return m_conn_size;
    }

    template <typename T>
    size_t ConnPool<T>::busy_size() const
    {
        return m_busy_conns.size();
    }

    template <typename T>
    void ConnPool<T>::set_min_size(size_t size)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_min_size = size <= m_max_size ? size : m_min_size;
    }

    template <typename T>
    void ConnPool<T>::set_max_size(size_t size)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_max_size = size >= m_min_size ? size : m_max_size;
    }

    template <typename T>
    void ConnPool<T>::check_connection()
    {
        // check state of idle conn until one is alive
        while (true)
        {
            conn_ptr conn;
            if (m_idle_conns.get(conn, std::chrono::milliseconds(0)))
            {
                if (conn->ping())
                {
                    m_idle_conns.put(std::move(conn));
                    break;
                }
                else
                {
                    --m_conn_size;
                    add_conn();
                }
            }
        }
    }

    template <typename T>
    void ConnPool<T>::check_scale()
    {
        static size_t exp_close = 0;
        static size_t idle_duration = 0;
        // close some conn if idle size remains for certain time
        //  2->3->3 close 2
        //  2->1->3 count from 1
        size_t idle_size = m_idle_conns.size();
        if (exp_close > idle_size)
        {
            idle_duration = 0;
            exp_close = idle_size;
        }
        else if (idle_duration + MONITOR_SLEEP_TIME >= MAX_IDLE_DURATION_TO_CLOSE_CONN)
        {
            size_t close_size = std::min(exp_close, m_conn_size - m_min_size);
            for (size_t i = 0; i < close_size; ++i)
            {
                conn_ptr tmp_conn;
                if (m_idle_conns.get(tmp_conn, std::chrono::milliseconds(0)))
                {
                    --m_conn_size;
                }
            }
            exp_close = 0;
            idle_duration = 0;
        }
        else if (MONITOR_SLEEP_TIME == (idle_duration += MONITOR_SLEEP_TIME))
        {
            exp_close = idle_size;
        }

        if (m_waiting_size > 0 || m_conn_size < m_min_size)
        {
            size_t add_size = std::min(m_waiting_size.load(), m_max_size - m_conn_size);
            for (size_t i = 0; i < add_size; i++)
            {
                add_conn();
            }
        }
    }

    template <typename T>
    void ConnPool<T>::release(conn_ptr &&conn)
    {
        if (m_busy_conns.erase(conn))
        {
            if (conn->ping())
            {
                m_idle_conns.put(std::move(conn));
            }
            else
            {
                --m_conn_size;
            }
        }
    }

    template <typename T>
    typename ConnPool<T>::conn_ptr ConnPool<T>::acquire()
    {
        ++m_waiting_size;
        while (true)
        {
            // try once non-blocking
            conn_ptr tmp_conn;
            if (!m_idle_conns.get(tmp_conn, std::chrono::milliseconds(0)))
            {
                add_conn();
                tmp_conn = m_idle_conns.get();
            }

            if (tmp_conn->ping())
            {
                --m_waiting_size;
                m_busy_conns.insert(tmp_conn);
                return tmp_conn;
            }
            else
            {
                --m_conn_size;
            }
        }
        return nullptr;
    }

    template <typename T>
    void ConnPool<T>::add_conn()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_conn_size >= m_max_size)
        {
            return;
        }

        conn_ptr conn = std::make_shared<T>();
        conn->set_conn_info(m_conn_str);
        if (conn->connect())
        {
            m_idle_conns.put(std::move(conn));
            ++m_conn_size;
        }
    }

    template <typename T>
    void ConnPool<T>::monitor()
    {

        while (!m_stop)
        {
            DEBUG_PRINT(*this);
            std::this_thread::sleep_for(std::chrono::milliseconds(MONITOR_SLEEP_TIME));
            check_scale();
            check_connection();
        }
    }

    template <typename T>
    void ConnPool<T>::init()
    {
        for (size_t i = 0; i < m_min_size; ++i)
        {
            add_conn();
        }

        m_monitor = std::move(std::thread(&ConnPool<T>::monitor, this));
    }

    template <typename T>
    ConnPool<T>::ConnPool(const std::string &conn_str, size_t min_size, size_t max_size) : m_conn_str(conn_str),
                                                                                           m_max_size(max_size),
                                                                                           m_min_size(std::min(min_size, max_size)),
                                                                                           m_conn_size(0),
                                                                                           m_waiting_size(0),
                                                                                           m_stop(false)
    {
        init();
    }

    template <typename T>
    ConnPool<T>::~ConnPool()
    {
        m_stop = true;
        if (m_monitor.joinable())
        {
            m_monitor.join();
        }
    }

} // namespace soda
