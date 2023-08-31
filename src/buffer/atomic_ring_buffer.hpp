#pragma once

// Ring buffer - heap memory; thread safe; default 16*1024 BYTES

#include <iostream>
#include <mutex>
#include <cstring>

#include "../general/util.hpp"

namespace soda
{
    class AtomicRingBuffer : Noncopyable
    {
    public:
        AtomicRingBuffer(size_t capacity = 16 * 1024);
        ~AtomicRingBuffer();

        // return the number of bytes written
        size_t write(const void *src, size_t size);

        // return the number of bytes read
        size_t read(void *dst, size_t size);

        bool full() const;

        bool empty() const;

        size_t size() const;

        size_t free_size() const;

        void clear();

        friend std::ostream &operator<<(std::ostream &os, const AtomicRingBuffer &rb)
        {
            return os << "capacity: " << rb.m_capacity
                      << " free_size: " << rb.free_size()
                      << " r_pos: " << rb.real_read_pos()
                      << " w_pos: " << rb.real_write_pos()
                      << std::endl;
        }

    private:
        // capacity 2^n Bytes
        size_t m_capacity;
        size_t m_write_pos;
        size_t m_read_pos;
        uint8_t *m_buffer;
        mutable std::mutex m_mtx;

    private:
        inline size_t real_read_pos() const;
        inline size_t real_write_pos() const;
    };

    AtomicRingBuffer::AtomicRingBuffer(size_t capacity) : m_capacity(roundup_pow_of_two(capacity)),
                                                          m_write_pos(0),
                                                          m_read_pos(0),
                                                          m_buffer(new uint8_t[m_capacity]{0})
    {
        if (!m_buffer)
        {
            ERROR_PRINT("AtomicRingBuffer init failed");
        }
    }

    AtomicRingBuffer::~AtomicRingBuffer()
    {
        if (m_buffer)
        {
            delete[] m_buffer;
            m_buffer = nullptr;
        }
    }

    // return the actual written length
    size_t AtomicRingBuffer::write(const void *src, size_t size)
    {
        if (size > free_size())
        {
            return 0;
        }

        std::lock_guard<std::mutex> lock(m_mtx);

        // obtain the writable space at the tail. If the space at the tail is insufficient, twice write is required. The minimum value is the length of the first write
        size_t write_size = std::min(size, m_capacity - real_write_pos());
        // the write point may be behind the read point, write twice
        memcpy(m_buffer + real_write_pos(), src, write_size);
        // The second write, if not needed, the last parameter is 0
        memcpy(m_buffer, reinterpret_cast<const uint8_t*>(src) + write_size, size - write_size);

        m_write_pos += size;

        return size;
    }

    // return the actual read length
    size_t AtomicRingBuffer::read(void *dst, size_t size)
    {
        if (empty())
        {
            return 0;
        }

        std::lock_guard<std::mutex> lock(m_mtx);

        // total readable quantity first time
        size_t read_size_sum = std::min(size, m_write_pos - m_read_pos);

        // get the readable length of the tail. If it is insufficient, it needs to be read twice. The minimum value is the read length of the tail
        size_t read_size_tail = std::min(read_size_sum, m_capacity - real_read_pos());
        memcpy(dst, m_buffer + real_read_pos(), read_size_tail);
        memcpy(reinterpret_cast<uint8_t*>(dst) + read_size_tail, m_buffer, read_size_sum - read_size_tail);

        m_read_pos += read_size_sum;

        return 0;
    }

    bool AtomicRingBuffer::full() const
    {
        return 0 == free_size();
    }

    bool AtomicRingBuffer::empty() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_read_pos == m_write_pos;
    }

    size_t AtomicRingBuffer::size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_write_pos - m_read_pos;
    }

    size_t AtomicRingBuffer::free_size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_capacity - (m_write_pos - m_read_pos);
    }

    void AtomicRingBuffer::clear()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_read_pos = m_write_pos = 0;
    }

    inline size_t AtomicRingBuffer::real_read_pos() const
    {
        return m_read_pos & (m_capacity - 1);
    }

    inline size_t AtomicRingBuffer::real_write_pos() const
    {
        return m_write_pos & (m_capacity - 1);
    }
} // namespace soda
