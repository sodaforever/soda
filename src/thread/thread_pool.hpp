#pragma once

// thread pool - automatically scale within min and max size; priority task

#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <list>
#include <iostream>
#include <condition_variable>
#include <future>
#include <atomic>

#include "../general/util.hpp"
#include "../queue/priority_task_queue.hpp"

namespace soda
{

    // ms
    constexpr size_t MONITOR_SLEEP_TIME = 5000;
    constexpr size_t MAX_IDLE_DURATION_TO_CLOSE_WORKER = 600000;

    class ThreadPool : Noncopyable
    {
    public:
        ThreadPool(size_t min_size = 1, size_t max_size = std::thread::hardware_concurrency());
        ~ThreadPool();

        // for restart mainly, constructor will start automaticlly
        void start();

        void stop();

        void set_min_size(size_t size);

        void set_max_size(size_t size);

        void add_new_worker(size_t num);

        template <typename F, typename... Args>
        auto insert_task(TaskPriority pri, F &&f, Args &&...args)
            -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>;

        template <typename F, typename... Args>
        auto insert_task_high(F &&f, Args &&...args)
            -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>;

        template <typename F, typename... Args>
        auto insert_task_normal(F &&f, Args &&...args)
            -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>;

        template <typename F, typename... Args>
        auto insert_task_low(F &&f, Args &&...args)
            -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>;

        size_t size() const;

        size_t busy_size() const;

        friend std::ostream &operator<<(std::ostream &os, const ThreadPool &tp)
        {
            return os << "thread_pool -"
                      << " all: " << tp.m_worker_size
                      << " busy: " << tp.m_busy_size
                      << " exp_close: " << tp.m_reduce_size
                      << " min: " << tp.m_min_size
                      << " max: " << tp.m_max_size
                      << std::endl;
        }

    private:
        size_t m_max_size;
        size_t m_min_size;
        std::atomic_size_t m_busy_size;
        // the number of worker threads need to be reduced
        std::atomic_size_t m_reduce_size;
        std::atomic_size_t m_worker_size;
        bool m_stop;
        std::unordered_map<std::thread::id, std::thread> m_workers;
        // thread to be joined
        std::list<std::thread> m_closed_workers;
        std::thread m_mgr;
        PriorityTaskQueue m_task_queue;

        std::mutex m_mtx;

    private:
        void worker_exit();

        void worker_proc();

        void add_worker_thread(size_t num);

        void manager_proc();

        // wait for workers need to be closed
        void wait_closed_workers();

        void wait_all_workers();

        inline void full_load() const;

        inline void init();

        void wakeup_worker(size_t num);

        void check_scale();
    };

    void ThreadPool::check_scale()
    {
        static size_t exp_close = 0;
        static size_t idle_duration = 0;
        // close some workers if idle size remains for certain time
        //  2->3->3 close 2
        //  2->1->3 count from 1
        size_t idle_size = m_worker_size - m_busy_size;
        if (exp_close > idle_size)
        {
            idle_duration = 0;
            exp_close = idle_size;
        }
        else if (idle_duration + MONITOR_SLEEP_TIME >= MAX_IDLE_DURATION_TO_CLOSE_WORKER)
        {
            m_reduce_size = std::min(exp_close, m_worker_size - m_min_size);
            exp_close = 0;
            idle_duration = 0;
            wakeup_worker(m_reduce_size);
            m_reduce_size = 0;
        }
        else if (MONITOR_SLEEP_TIME == (idle_duration += MONITOR_SLEEP_TIME))
        {
            exp_close = idle_size;
        }

        if (m_busy_size == m_worker_size && m_worker_size < m_max_size && !m_task_queue.empty())
        {
            add_worker_thread(std::min(m_max_size - m_worker_size, m_task_queue.size()));
        }
        else if (m_worker_size == m_busy_size && m_worker_size == m_max_size && !m_task_queue.empty())
        {
            full_load();
        }
    }

    void ThreadPool::set_min_size(size_t size)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_min_size = size <= m_max_size ? size : m_min_size;
    }

    void ThreadPool::set_max_size(size_t size)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_max_size = size >= m_min_size ? size : m_max_size;
    }

    void ThreadPool::add_new_worker(size_t num)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_max_size += num;
    }

    ThreadPool::ThreadPool(size_t min_size, size_t max_size) : m_max_size(max_size),
                                                               m_min_size(std::min(min_size, max_size)),
                                                               m_busy_size(0),
                                                               m_reduce_size(0),
                                                               m_worker_size(0),
                                                               m_stop(false)
    {
        init();
    }

    ThreadPool::~ThreadPool()
    {
        stop();
    }
    void ThreadPool::start()
    {
        if (m_stop)
        {
            m_stop = false;
            init();
        }
    }

    void ThreadPool::stop()
    {
        m_stop = true;
        if (m_mgr.joinable())
        {
            m_mgr.join();
        }
    }

    size_t ThreadPool::size() const
    {
        return m_worker_size;
    }

    size_t ThreadPool::busy_size() const
    {
        return m_busy_size;
    }

    void ThreadPool::worker_exit()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_closed_workers.emplace_back(std::move(m_workers.at(std::this_thread::get_id())));
        m_workers.erase(std::this_thread::get_id());
    }

    void ThreadPool::worker_proc()
    {
        while (!m_stop)
        {
            TaskType task = m_task_queue.dequeue();
            ++m_busy_size;

            //!!! thread can't go out when loop in tsak
            task();

            if (m_reduce_size > 0)
            {
                worker_exit();
                --m_reduce_size;
                --m_worker_size;
                --m_busy_size;
                break;
            }
            --m_busy_size;
        }
        worker_exit();
        --m_worker_size;
    }

    void ThreadPool::add_worker_thread(size_t num)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        for (size_t i = 0; i < num; ++i)
        {
            std::thread t(&ThreadPool::worker_proc, this);
            m_workers.emplace(t.get_id(), std::move(t));
            ++m_worker_size;
        }
    }

    void ThreadPool::manager_proc()
    {
        add_worker_thread(m_min_size);

        while (!m_stop)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(MONITOR_SLEEP_TIME));

            check_scale();
            wait_closed_workers();
        }
        wait_all_workers();
    }

    void ThreadPool::wait_closed_workers()
    {

        for (std::thread &worker : m_closed_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        std::lock_guard<std::mutex> lock(m_mtx);
        m_closed_workers.clear();
    }

    void ThreadPool::wait_all_workers()
    {
        wakeup_worker(m_max_size);
        wait_closed_workers();

        std::lock_guard<std::mutex> lock(m_mtx);
        m_worker_size = 0;
        m_busy_size = 0;
        m_reduce_size = 0;
    }

    inline void ThreadPool::full_load() const
    {
        DEBUG_PRINT("thread pool full load");
    }

    inline void ThreadPool::init()
    {
        m_mgr = std::thread(&ThreadPool::manager_proc, this);
    }

    template <typename F, typename... Args>
    auto ThreadPool::insert_task(TaskPriority pri, F &&f, Args &&...args)
        -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        using ret_type = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        // packaged_task is noncopyable，but std::function requires copyable one，so wrapped in ptr
        std::shared_ptr<std::packaged_task<ret_type()>> task = std::make_shared<std::packaged_task<ret_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ret_type> ret = task->get_future();
        m_task_queue.enqueue([task]()
                             { (*task)(); },
                             pri);
        return ret;
    }

    template <typename F, typename... Args>
    auto ThreadPool::insert_task_high(F &&f, Args &&...args)
        -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        return insert_task(TaskPriority::HIGH, std::forward<F>(f), std::forward<Args>(args)...);
    }

    template <typename F, typename... Args>
    auto ThreadPool::insert_task_normal(F &&f, Args &&...args)
        -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        return insert_task(TaskPriority::NORMAL, std::forward<F>(f), std::forward<Args>(args)...);
    }

    template <typename F, typename... Args>
    auto ThreadPool::insert_task_low(F &&f, Args &&...args)
        -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        return insert_task(TaskPriority::LOW, std::forward<F>(f), std::forward<Args>(args)...);
    }

    void ThreadPool::wakeup_worker(size_t num)
    {
        for (size_t i = 0; i < num; ++i)
        {
            m_task_queue.enqueue([] {});
        }
    }

} // namespace soda
