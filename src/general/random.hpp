#pragma once

// random related - interger, float, string

#include <random>

namespace soda
{
    namespace random
    {
        /// Get an integer random number
        /// @param min minimum value
        /// @param max maximum value
        /// [min,max] closed interval
        int64_t get_int(int64_t min, int64_t max)
        {
            std::default_random_engine e;
            std::uniform_int_distribution<> u(min, max);
            std::random_device rd;
            e.seed(rd());
            return u(e);
        }

        /// Get real random number
        /// @param min minimum value
        /// @param max maximum value
        /// [min,max] closed interval
        double get_real(double min, double max)
        {
            std::default_random_engine e;
            std::uniform_real_distribution<> u(min, max);
            std::random_device rd;
            e.seed(rd());
            return u(e);
        }

        /// Get a normal distribution random number
        /// @param mean average value
        /// @param stddev standard deviation
        double get_normal_distribution(double mean, double stddev)
        {
            std::default_random_engine e;
            std::normal_distribution<> u(mean, stddev);
            std::random_device rd;
            e.seed(rd());
            return u(e);
        }

        /// Get bool
        /// @param p returns the probability of true
        bool get_bool(double p)
        {
            std::default_random_engine e;
            std::bernoulli_distribution u(p);
            std::random_device rd;
            e.seed(rd());
            return u(e);
        }

#define MIN_LETTER_UPPER 65
#define MAX_LETTER_UPPER 90
#define MIN_LETTER_LOWER 97
#define MAX_LETTER_LOWER 122
#define MIN_NUM_CHAR 48
#define MAX_NUM_CHAR 57
        // Get a random upper letter
        char get_upper_letter()
        {
            return get_int(MIN_LETTER_UPPER, MAX_LETTER_UPPER);
        }

        // Get a random lower letter
        char get_lower_letter()
        {
            return get_int(MIN_LETTER_LOWER, MAX_LETTER_LOWER);
        }

        // Get a random number character
        char get_num_char()
        {
            return get_int(MIN_NUM_CHAR, MAX_NUM_CHAR);
        }

        /// Get a random string
        /// @param size string length
        /// @param upper_letter whether to contain uppercase letters
        /// @param lower_letter whether to contain lowercase letters
        /// @param num contains numbers
        std::string get_str(size_t size, bool upper_letter = true, bool lower_letter = true, bool num = true)
        {
            std::vector<char (*)()> gen;
            if (upper_letter)
            {
                gen.emplace_back(get_upper_letter);
            }

            if (lower_letter)
            {
                gen.emplace_back(get_lower_letter);
            }

            if (num)
            {
                gen.emplace_back(get_num_char);
            }

            if (0 == gen.size())
            {
                return std::move(std::string());
            }

            std::string ret;
            for (size_t i = 0; i < size; ++i)
            {
                ret += gen[get_int(0, gen.size() - 1)]();
            }
            return ret;
        }

    } // namespace random
} // namespace soda
