#pragma once

// error

#include <string>
#include <errno.h>
#include <stdint.h>

namespace soda
{
    enum ErrorLevel
    {
        NONE,
        WARNING,
        FATAL
    };
    struct Error
    {
        ErrorLevel level;
        int32_t code;
        std::string message;
    };

} // namespace soda
