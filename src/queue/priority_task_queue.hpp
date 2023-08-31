#pragma once

// multi-thread priority task queue

#include <list>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "../general/util.hpp"

namespace soda
{
    using TaskType = std::function<void()>;

    enum TaskPriority
    {
        HIGH,
        NORMAL,
        LOW
    };

    class PriorityTaskQueue : Noncopyable
    {
    public:
        PriorityTaskQueue() : m_size(0) {}
        ~PriorityTaskQueue() {}

        bool empty() const;

        size_t size() const;
        size_t size(TaskPriority pri) const;

        void enqueue(TaskType task, TaskPriority pri = TaskPriority::NORMAL);

        TaskType dequeue();

    private:
        // low q
        std::list<TaskType> m_queue_low;
        // normal q
        std::list<TaskType> m_queue_normal;
        // high q
        std::list<TaskType> m_queue_high;

        std::mutex m_mtx;
        std::condition_variable m_cv;

        size_t m_size;
    };

    bool PriorityTaskQueue::empty() const
    {
        return !m_size;
    }

    size_t PriorityTaskQueue::size() const
    {
        return m_size;
    }

    size_t PriorityTaskQueue::size(TaskPriority pri) const
    {
        switch (pri)
        {
        case HIGH:
            return m_queue_high.size();
        case NORMAL:
            return m_queue_normal.size();
        case LOW:
            return m_queue_low.size();
        default:
            break;
        }
        return m_size;
    }

    void PriorityTaskQueue::enqueue(TaskType task, TaskPriority pri)
    {
        std::list<TaskType> *queue = &m_queue_normal;
        if (pri == TaskPriority::LOW)
        {
            queue = &m_queue_low;
        }
        else if (pri == TaskPriority::HIGH)
        {
            queue = &m_queue_high;
        }

        std::lock_guard<std::mutex> lock(m_mtx);
        queue->emplace_back(std::move(task));
        ++m_size;

        m_cv.notify_one();
    }

    TaskType PriorityTaskQueue::dequeue()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [this]
                  { return !this->empty(); });

        --m_size;
        if (!m_queue_high.empty())
        {
            // libc++abi: terminating with uncaught exception of type std::__1::future_error: The state of the promise has already been set.
            // auto &ret = m_queue_high.front();
            TaskType ret = std::move(m_queue_high.front());
            m_queue_high.pop_front();
            return ret;
        }

        if (!m_queue_normal.empty())
        {
            TaskType ret = std::move(m_queue_normal.front());
            m_queue_normal.pop_front();
            return ret;
        }

        if (!m_queue_low.empty())
        {
            TaskType ret = std::move(m_queue_low.front());
            m_queue_low.pop_front();
            return ret;
        }
        return std::move([] {});
    }

} // namespace soda
