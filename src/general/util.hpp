#pragma once

// general utils

#include <stdio.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <chrono>
#include <assert.h>
#include <type_traits>

#include "noncopyable.hpp"

#ifndef NDEBUG

#define ERROR_PRINT(x) std::cerr << x

#define DEBUG_PRINT(x) std::cout << x << std::endl;

#define PRINT(x) std::cout << x

#define PRINT_WITH_LINE(x) std::cout << x << '\n'

#define PRINT_WITH_SPACE(x) std::cout << x << ' '

#define PRINT_WITH_TAB(x) std::cout << x << '\t'

#define PRINT_WITH_DIVIDER(x) std::cout << x << "\n-----------------------------------------------------------------------------------------------------------------------------------" << '\n'

#define PRINT_SPACE std::cout << ' '

#define PRINT_LINE std::cout << '\n'

#define PRINT_DIVIDER std::cout << "\n-----------------------------------------------------------------------------------------------------------------------------------" << '\n'

#else
#define ERROR_PRINT(x)
#define DEBUG_PRINT(x)
#define PRINT(x)
#define PRINT_WITH_LINE(x)
#define PRINT_WITH_SPACE(x)
#define PRINT_WITH_TAB(x)
#define PRINT_WITH_DIVIDER(x)
#define PRINT_SPACE
#define PRINT_LINE
#define PRINT_DIVIDER
#endif

namespace soda
{
    // whether it is a power of 2
    bool is_pow_of_two(uint64_t num)
    {
        return (num > 0) && (0 == (num & (num - 1)));
    }

    // take the smallest upper bound 2^n
    uint64_t roundup_pow_of_two(uint64_t num)
    {
        if (is_pow_of_two(num))
        {
            return num;
        }

        uint64_t ret = 1;
        while (ret < num)
        {
            ret <<= 1;
        }

        return ret;
    }

#ifdef CLOCK_OPEN
    // time expense monitor
    class Clock
    {
    public:
        Clock(const std::string &name = "", bool destruct_calculate = true) : m_start(std::chrono::steady_clock::now()),
                                                                              m_pause(m_start),
                                                                              m_name(name),
                                                                              m_destruct_calculate(destruct_calculate) {}

        void stop(const std::string &note = "")
        {
#if CLOCK_OPEN
            m_stop = std::chrono::steady_clock::now();
            auto duration = m_stop - m_pause;
            double ms = duration.count() * 0.000001;
            if (note.size())
                PRINT_WITH_DIVIDER("[" << m_name << " - " << note << "] : " << ms << "(ms)");
            else if (m_name.size())
                PRINT_WITH_DIVIDER("[" << m_name << "] : " << ms << "(ms)");
            else
                PRINT_WITH_DIVIDER(ms << "(ms)");
            m_pause = std::chrono::steady_clock::now();
#endif
        }

        void non_inter_stop(const std::string &note = "")
        {
            m_stop = std::chrono::steady_clock::now();
            auto duration = m_stop - m_pause;
            double ms = duration.count() * 0.000001;
            if (note.size())
                PRINT_WITH_DIVIDER("[" << m_name << " - " << note << "] : " << ms << "(ms)");
            else if (m_name.size())
                PRINT_WITH_DIVIDER("[" << m_name << "] : " << ms << "(ms)");
            else
                PRINT_WITH_DIVIDER(ms << "(ms)");
            m_pause = std::chrono::steady_clock::now();
        }

        ~Clock()
        {
            if (m_destruct_calculate)
            {
                m_pause = m_start;
                stop();
            }
        }

    private:
        std::chrono::time_point<std::chrono::steady_clock> m_start;
        std::chrono::time_point<std::chrono::steady_clock> m_pause;
        std::chrono::time_point<std::chrono::steady_clock> m_stop;
        std::string m_name;
        bool m_destruct_calculate;
    };
#endif

    template <typename T>
    using rm_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    template <bool B, typename T = void>
    using enable_if_t = typename std::enable_if<B, T>::type;

    template <typename cond>
    using require = enable_if_t<cond::value>;
} // namespace soda
