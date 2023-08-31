#pragma once

// socket error

#include "../general/error.hpp"

namespace soda
{
    struct SocketError
    {
        int32_t code;
        std::string msg = strerror(code);

        bool fatal();
    };

    bool SocketError::fatal()
    {
        switch (code)
        {
        // Errors that might require socket closure
        case EACCES:
        case EADDRNOTAVAIL:
        case EBADF:
        case ECONNABORTED:
        case ECONNRESET:
        case EFAULT:
        case EINVAL:
        case EISCONN:
        case ENETDOWN:
        case ENETRESET:
        case ENOPROTOOPT:
        case ENOTCONN:
        case ENOTSOCK:
        case EOPNOTSUPP:
        case EPROTO: // Protocol error
        case EPROTONOSUPPORT:
            return true;

        // Errors that might be ignored or might only require logging
        case EALREADY:
        case EINPROGRESS:
        case EDESTADDRREQ:
            return false;

        // Errors that might hint at a retry
        case EADDRINUSE:
        case EAGAIN | EWOULDBLOCK: // Often synonymous with EWOULDBLOCK
        case EHOSTUNREACH:
        case ENETUNREACH:
        case ENOBUFS:
        case ETIMEDOUT:
            return false;

        // Errors that might require logging and then continuing the operation
        case EINTR: // Interrupted system call
            return false;

        // Errors that are serious and might require shutting down the application
        case EMFILE: // Too many open files
            return true;

        // Default action, you might want to extend or modify the list
        default:
            return true;
        }
    }

} // namespace soda
