#pragma once

// blocking queue

#include <list>
#include <mutex>
#include <condition_variable>

#include "../general/util.hpp"
namespace soda
{
    template <typename T>
    class BlockingQueue : Noncopyable
    {
    private:
        std::list<T> m_container;

        mutable std::mutex m_mtx;
        std::condition_variable m_empty_cv;

    public:
        BlockingQueue();
        ~BlockingQueue();

        void put(T &&src);
        void put(std::initializer_list<T> &&src);

        T get();

        // false if timeout
        bool get(T &dst, std::chrono::milliseconds timeout);

        bool empty() const;

        size_t size() const;

        void clear();
    };

    template <typename T>
    BlockingQueue<T>::BlockingQueue() {}

    template <typename T>
    BlockingQueue<T>::~BlockingQueue() {}

    template <typename T>
    void BlockingQueue<T>::put(T &&src)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_container.emplace_back(std::forward<T>(src));
        m_empty_cv.notify_one();
    }

    template <typename T>
    void BlockingQueue<T>::put(std::initializer_list<T> &&src)
    {
        for (auto &&ele : src)
        {
            put(std::forward<T>(ele));
        }
    }

    template <typename T>
    T BlockingQueue<T>::get()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_empty_cv.wait(lock, [this]
                        { return !this->m_container.empty(); });
        T ret = std::move(m_container.front());
        m_container.pop_front();
        return ret;
    }

    template <typename T>
    bool BlockingQueue<T>::get(T &dst, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (m_empty_cv.wait_for(lock, timeout, [this]
                                { return !this->m_container.empty(); }))
        {
            dst = std::move(m_container.front());
            m_container.pop_front();
            return true;
        }

        return false;
    }

    template <typename T>
    bool BlockingQueue<T>::empty() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_container.empty();
    }

    template <typename T>
    size_t BlockingQueue<T>::size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_container.size();
    }

    template <typename T>
    void BlockingQueue<T>::clear()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_container.clear();
    }
} // namespace soda
