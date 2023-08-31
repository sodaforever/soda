#pragma once

// bounded blocking queue

#include <list>
#include <mutex>
#include <condition_variable>

#include "../general/util.hpp"
namespace soda
{
    template <typename T>
    class BoundedBlockingQueue : Noncopyable
    {
    private:
        std::list<T> m_container;
        mutable std::mutex m_mtx;
        std::condition_variable m_full_cv;
        std::condition_variable m_empty_cv;
        size_t m_max_size;

    public:
        BoundedBlockingQueue(size_t max_size = -1);
        ~BoundedBlockingQueue();

        void put(T &&t);
        // false if timeout
        bool put(T &&t, std::chrono::milliseconds timeout);
        void put(std::initializer_list<T> &&list);

        T get();
        // false if timeout
        bool get(T &dst, std::chrono::milliseconds timeout);

        bool empty() const;

        size_t size() const;

        void clear();
    };

    template <typename T>
    BoundedBlockingQueue<T>::BoundedBlockingQueue(size_t max_size) : m_max_size(max_size) {}

    template <typename T>
    BoundedBlockingQueue<T>::~BoundedBlockingQueue() {}

    template <typename T>
    void BoundedBlockingQueue<T>::put(T &&t)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_full_cv.wait(lock, [this]
                       { return this->m_container.size() < this->m_max_size; });

        m_container.emplace_back(std::forward<T>(t));
        m_empty_cv.notify_one();
    }

    template <typename T>
    bool BoundedBlockingQueue<T>::put(T &&t, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_full_cv.wait(lock, [this]
                       { return this->m_container.size() < this->m_max_size; });

        m_container.emplace_back(std::forward<T>(t));
        m_empty_cv.notify_one();
    }

    template <typename T>
    void BoundedBlockingQueue<T>::put(std::initializer_list<T> &&list)
    {
        for (auto &&ele : v)
        {
            put(std::forward<T>(i))
        }
    }

    template <typename T>
    T BoundedBlockingQueue<T>::get()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_empty_cv.wait(lock, [this]
                        { return !this->m_container.empty(); });

        T ret = std::move(m_container.front());
        m_container.pop_front();
        m_full_cv.notify_one();
        return ret;
    }

    template <typename T>
    bool BoundedBlockingQueue<T>::get(T &dst, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (m_empty_cv.wait_for(m_mtx, timeout, [this]
                                { return !this->m_container.empty(); }))
        {
            dst = std::move(m_container.front());
            m_container.pop_front();
            m_full_cv.notify_one();
            return true;
        }
        return false;
    }

    template <typename T>
    bool BoundedBlockingQueue<T>::empty() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_container.empty();
    }

    template <typename T>
    size_t BoundedBlockingQueue<T>::size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_container.size();
    }

    template <typename T>
    void BoundedBlockingQueue<T>::clear()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_container.clear();
    }
} // namespace soda
