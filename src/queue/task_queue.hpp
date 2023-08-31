#pragma once

// multi-thread safe

#include <list>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "../general/util.hpp"

namespace soda
{
    using TaskType = std::function<void()>;

    class TaskQueue : Noncopyable
    {
    public:
        TaskQueue() {}
        ~TaskQueue() {}

        bool empty() const;

        size_t size() const;

        void clear();

        void enqueue(TaskType task);

        TaskType dequeue();

    private:
        std::list<TaskType> m_container;

        mutable std::mutex m_mtx;
        std::condition_variable m_cv;
    };

    bool TaskQueue::empty() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_container.empty();
    }

    size_t TaskQueue::size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_container.size();
    }

    void TaskQueue::clear()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_container.clear();
    }

    void TaskQueue::enqueue(TaskType task)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_container.emplace_back(std::move(task));

        m_cv.notify_one();
    }

    TaskType TaskQueue::dequeue()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [this]
                  { return !this->m_container.empty(); });

        // package_task wrapped in std::function
        // libc++abi: terminating with uncaught exception of type std::__1::future_error: The state of the promise has already been set.
        // auto &ret = m_container_high.front();
        TaskType ret = std::move(m_container.front());
        m_container.pop_front();
        return ret;
    }
} // namespace soda
