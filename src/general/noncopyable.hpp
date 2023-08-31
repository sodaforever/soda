#pragma once

namespace soda
{
    class Noncopyable
    {
    public:
        Noncopyable(const Noncopyable &) = delete;
        void operator=(const Noncopyable &) = delete;

    protected:
        Noncopyable(){};
        ~Noncopyable(){};
    };
} // namespace soda
