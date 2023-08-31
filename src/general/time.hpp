#pragma once

// Time tools - UTC and local time, timestamp, string; format conversion; time comparison; time addition and subtraction

#include <chrono>
#include <ctime>
#include <string>
#include <iomanip>
#include <sstream>
#include <thread>

namespace soda
{

    namespace time
    {
        using Clock = std::chrono::system_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        // get the current UTC time
        static TimePoint now()
        {
            return Clock::now();
        }

        // get the timestamp of the current time in seconds
        static int64_t to_ts_sec(const TimePoint &tp)
        {
            return std::chrono::duration_cast<std::chrono::seconds>(tp. time_since_epoch()). count();
        }

        // get the timestamp of the current time in milliseconds
        static int64_t to_ts_millisec(const TimePoint &tp)
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(tp. time_since_epoch()). count();
        }

        // timestamp converted to TimePoint
        static TimePoint from_ts_sec(int64_t seconds)
        {
            return TimePoint(std::chrono::seconds(seconds));
        }

        static TimePoint from_ts_millisec(int64_t milliseconds)
        {
            return TimePoint(std::chrono::milliseconds(milliseconds));
        }

        // get the string of the current UTC time
        static std::string UTC_string(const TimePoint &tp)
        {
            time_t tt = Clock::to_time_t(tp);
            std::tm gmt;
            gmtime_r(&tt, &gmt);
            char buffer[50];
            strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", &gmt);
            return std::string(buffer);
        }

        // get the string of the current local time
        static std::string local_string(const TimePoint &tp)
        {
            time_t tt = Clock::to_time_t(tp);
            std::tm local_tm;
            localtime_r(&tt, &local_tm);
            char buffer[50];
            strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", &local_tm);
            return std::string(buffer);
        }

        // time comparison
        static bool is_before(const TimePoint &tp1, const TimePoint &tp2)
        {
            return tp1 < tp2;
        }

        static bool is_after(const TimePoint &tp1, const TimePoint &tp2)
        {
            return tp1 > tp2;
        }

        static bool is_equal(const TimePoint &tp1, const TimePoint &tp2)
        {
            return tp1 == tp2;
        }

        // Check if it is a leap year
        static bool is_leapyear(int year)
        {
            if (year % 4 != 0)
                return false;
            if (year % 100 != 0)
                return true;
            if (year % 400 != 0)
                return false;
            return true;
        }

        // sleep
        static void sleep_sec(int seconds)
        {
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
        }

        static void sleep_millisec(int milliseconds)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }

        // time addition and subtraction
        static TimePoint add_sec(const TimePoint &tp, int seconds)
        {
            return tp + std::chrono::seconds(seconds);
        }

        static TimePoint sub_sec(const TimePoint &tp, int seconds)
        {
            return tp - std::chrono::seconds(seconds);
        }

        static TimePoint add_millisec(const TimePoint &tp, int milliseconds)
        {
            return tp + std::chrono::milliseconds(milliseconds);
        }

        static TimePoint sub_millisec(const TimePoint &tp, int milliseconds)
        {
            return tp - std::chrono::milliseconds(milliseconds);
        }

        // get year, month, day information from TimePoint
        static void getYMD(const TimePoint &tp, int &year, int &month, int &day)
        {
            time_t tt = Clock::to_time_t(tp);
            std::tm gmt;
            gmtime_r(&tt, &gmt); // use UTC time
            year = gmt.tm_year + 1900; // because tm_year starts from 1900
            month = gmt.tm_mon + 1; // because tm_mon starts from 0
            day = gmt.tm_mday;
        }

    } // namespace time

} // namespace soda