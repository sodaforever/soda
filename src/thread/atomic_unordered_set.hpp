#pragma once

// multi-thread safe unordered set

#include <unordered_set>
#include <mutex>

namespace soda
{

    template <typename T>
    class AtomicUnorderedSet
    {
    private:
        std::unordered_set<T> m_set;
        mutable std::mutex m_mtx;

    public:
        bool insert(const T &value);
        bool erase(const T &value);
        bool contains(const T &value) const;
        size_t size() const;
        bool empty() const;
        void clear();
    };

    template <typename T>
    bool AtomicUnorderedSet<T>::insert(const T &value)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_set.insert(value).second;
    }

    template <typename T>
    bool AtomicUnorderedSet<T>::erase(const T &value)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_set.erase(value) > 0;
    }

    template <typename T>
    bool AtomicUnorderedSet<T>::contains(const T &value) const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_set.find(value) != m_set.end();
    }

    template <typename T>
    size_t AtomicUnorderedSet<T>::size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_set.size();
    }

    template <typename T>
    bool AtomicUnorderedSet<T>::empty() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_set.empty();
    }

    template <typename T>
    void AtomicUnorderedSet<T>::clear()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_set.clear();
    }

} // namespace soda
