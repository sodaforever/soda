#pragma once

// epoll event driver

#include <sys/epoll.h>
#include <stdio.h>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>
#include <sys/eventfd.h>
#include <unistd.h>
#include <memory>

namespace soda
{

    class Epoller
    {
        // The maximum number of fds processed at a time
        static const size_t EPOLL_MAX_ONCE_WAKEUP = 100;

    private:
        int32_t m_epfd;
        // wakeup fd
        int32_t m_wfd;
        std::unordered_set<int> m_fds;
        std::atomic_bool m_is_listening;
        std::atomic_bool m_stop;
        std::mutex m_mtx;
        std::shared_ptr<epoll_event> m_events;

    public:
        Epoller();
        ~Epoller();

        // The first value of failure is -1, and then the number of events is returned successfully
        using check_res_t = std::tuple<int32_t, std::shared_ptr<epoll_event>>;
        check_res_t check_once();

        // for restart mainly, constructor will start automaticlly
        void start();

        void stop();

        // -1 if failed; if it exists, it is seen as success
        int add_event(int32_t fd, int events);

        // -1 if failed, if it does not exist, it is regarded as success
        int del_event(int32_t fd);

        // -1 if failed, if it does not exist, it is considered as failure
        int mod_event(int32_t fd, int events);

    private:
        // -1 if failed
        int init();

        // wake up epoll
        void wakeup();
    };

    // -1 if failed
    int Epoller::init()
    {
        m_epfd = epoll_create1(EPOLL_CLOEXEC);
        if (-1 == m_epfd)
        {
            perror("epoll create failed");
            return -1;
        }

        m_wfd = eventfd(0, EFD_NONBLOCK);
        epoll_event ev;
        ev.data.fd = -1;
        ev.events = EPOLLIN | EPOLLET;
        int ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_wfd, &ev);
        if (-1 == ret)
        {
            perror("epoll add eventfd failed");
            return -1;
        };

        m_events = std::shared_ptr<epoll_event>(new epoll_event[EPOLL_MAX_ONCE_WAKEUP],
                                                [](epoll_event *p)
                                                {delete []p;p = nullptr; });
        m_stop = false;
        return 0;
    }

    // wake up epoll
    void Epoller::wakeup()
    {
        // uint64_t val = 1;
        // ssize_t ret = write(m_wfd, &val, sizeof(uint64_t));

        if (-1 == eventfd_write(m_wfd, 1))
        {
            DEBUG_PRINT("eventfd_write failed");
        }
    }

    // The first value of failure is -1, and then the number of events is returned successfully
    Epoller::check_res_t Epoller::check_once()
    {
        check_res_t result{-1, m_events};
        if (m_is_listening || m_stop)
        {
            return result;
        }
        m_is_listening = true;
        int ret = -1;
        do
        {
            ret = epoll_wait(m_epfd, m_events.get(), EPOLL_MAX_ONCE_WAKEUP, -1);
        } while (-1 == ret && EINTR == errno);

        if (-1 == ret)
        {
            perror("epoll wait failed");
        }
        std::get<0>(result) = ret;
        m_is_listening = false;
        return result;
    }

    void Epoller::start()
    {
        if (!m_stop)
        {
            return;
        }
        init();
    }

    void Epoller::stop()
    {
        if (m_stop)
        {
            return;
        }
        m_stop = true;
        wakeup();
        while (m_is_listening)
        {
            // wait for epoll wake up from wait
        }
        close(m_epfd);
        close(m_wfd);
        m_fds.clear();
    }

    // -1 if failed; if it exists, it is seen as success
    int Epoller::add_event(int32_t fd, int events)
    {
        std::lock_guard<std::mutex> lock(m_mtx);

        if (m_fds.find(fd) != m_fds.end())
        {
            return 0;
        }

        epoll_event ev;
        ev.data.fd = fd;
        ev.events = events;

        int ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev);
        if (-1 == ret)
        {
            perror("epoll add failed");
            return -1;
        }

        m_fds.insert(fd);
        return ret;
    }

    // -1 if failed, if it does not exist, it is regarded as success
    int Epoller::del_event(int32_t fd)
    {
        std::lock_guard<std::mutex> lock(m_mtx);

        if (m_fds.find(fd) == m_fds.end())
        {
            return 1;
        }

        int ret = epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
        if (-1 == ret)
        {
            perror("epoll del failed");
            return -1;
        }
        m_fds.erase(fd);
        return ret;
    }

    // -1 if failed, if it does not exist, it is considered as failure
    int Epoller::mod_event(int32_t fd, int events)
    {
        std::lock_guard<std::mutex> lock(m_mtx);

        if (m_fds.find(fd) == m_fds.end())
        {
            perror("epoll mod failed, fd does not exist");
            return -1;
        }

        epoll_event ev;
        ev.data.fd = fd;
        ev.events = events;
        int ret = epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &ev);

        if (-1 == ret)
        {
            perror("epoll mod failed");
            return -1;
        }
        return ret;
    }

    Epoller::Epoller() : m_epfd(-1), m_wfd(-1), m_is_listening(false), m_stop(true), m_events(nullptr)
    {
        init();
    }

    Epoller::~Epoller()
    {
        stop();
    }

} // namespace soda