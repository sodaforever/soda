#pragma once

// thread pool - fixed size; no priority

#include <functional>
#include <thread>
#include <iostream>
#include <future>
#include <list>
#include <atomic>

#include "../queue/task_queue.hpp"
#include "../general/util.hpp"

namespace soda
{
    class SimpleThreadPool : Noncopyable
    {
    public:
        SimpleThreadPool(size_t size);
        ~SimpleThreadPool();

        template <typename F, typename... Args>
        auto insert_task(F &&f, Args &&...args)
            -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>;

        void stop();

        size_t size() const;

        size_t busy_size() const;

        friend std::ostream &operator<<(std::ostream &os, const SimpleThreadPool &tp)
        {
            return os << "thread all: " << tp.size() << " busy: " << tp.busy_size() << std::endl;
        }

    private:
        size_t m_size;
        std::atomic_size_t m_busy_size;
        bool m_stop;
        std::list<std::thread> m_workers;
        TaskQueue m_task_queue;

        void wait_all();

        void worker_proc();

        void init();
    };

    SimpleThreadPool::SimpleThreadPool(size_t size) : m_size(size), m_stop(false)
    {
        init();
    }

    SimpleThreadPool::~SimpleThreadPool()
    {
        stop();
    }

    void SimpleThreadPool::wait_all()
    {
        for (auto &&worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        m_workers.clear();
        m_size = 0;
    }

    void SimpleThreadPool::stop()
    {
        m_stop = true;
        for (size_t i = 0; i < m_size; ++i)
        {
            // wakeup
            m_task_queue.enqueue([] {});
        }
        wait_all();
    }

    template <typename F, typename... Args>
    auto SimpleThreadPool::insert_task(F &&f, Args &&...args)
        -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        using ret_type = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        // packaged_task is noncopyable，but std::function requires copyable one，so wrapped in ptr
        std::shared_ptr<std::packaged_task<ret_type()>> task = std::make_shared<std::packaged_task<ret_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ret_type> ret = task->get_future();
        m_task_queue.enqueue([task]()
                             { (*task)(); });
        return ret;
    }

    size_t SimpleThreadPool::size() const
    {
        return m_workers.size();
    }

    size_t SimpleThreadPool::busy_size() const
    {
        return m_busy_size;
    }

    void SimpleThreadPool::worker_proc()
    {
        while (true)
        {
            TaskType task = m_task_queue.dequeue();
            if (m_stop)
            {
                break;
            }

            ++m_busy_size;
            //!!! thread can't go out when loop in tsak
            task();
            --m_busy_size;
        }
    }

    void SimpleThreadPool::init()
    {
        for (size_t i = 0; i < m_size; ++i)
        {
            std::thread t(&SimpleThreadPool::worker_proc, this);
            m_workers.emplace_back(std::move(t));
        }
    }

} // namespace soda

