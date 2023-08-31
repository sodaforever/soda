#pragma once

// tls related

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <memory>

#include "../general/util.hpp"

namespace soda
{
    struct SSLDeleter
    {
        void operator()(SSL *ssl)
        {
            if (ssl)
            {
                // as to manual need to check the state of fd
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
        }
    };

    class TLSUtil : Noncopyable
    {
    private:
        SSL_CTX *m_ctx;
        bool m_is_server;

    public:
        using ssl_ptr = std::shared_ptr<SSL>;
        TLSUtil(bool is_server);
        ~TLSUtil();

        bool set_crt_key(const std::string &cert, const std::string &key, int file_type = SSL_FILETYPE_PEM) const;

        bool set_CA(const std::string &cert) const;

        // nullptr if failed
        ssl_ptr get_ssl(uint32_t fd);

        // set if need check peer's cert
        void set_if_verify_peer_crt(bool verify) const;

        // for server; 1 successful; 0 try later; -1 error
        int accept(const ssl_ptr &ssl) const;

        // for client; 1 successful; 0 try later; -1 error
        int connect(const ssl_ptr &ssl) const;

        // -1 if failed or disconnected; received length on success; 0 try later
        int recv(const ssl_ptr &ssl, void *dst, size_t size) const;

        // -1 if failed or disconnected; sent length on success; 0 try later
        int send(const ssl_ptr &ssl, const void *src, size_t size) const;

    private:
        void init();
        void cleanup();

        int handle_err(const SSL *ssl, int err) const;
    };

    int TLSUtil::handle_err(const SSL *ssl, int err) const
    {
        err = SSL_get_error(ssl, err);
        switch (err)
        {
        case SSL_ERROR_NONE:
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return 0;
        case SSL_ERROR_SYSCALL:
            return errno == EINTR ? 0 : -1;
        default:
            ERR_print_errors_fp(stderr);
            return -1;
        }
    }

    int TLSUtil::send(const ssl_ptr &ssl, const void *src, size_t size) const
    {
        int ret = SSL_write(ssl.get(), src, size);
        return ret > 0 ? ret : handle_err(ssl.get(), ret);
    }

    int TLSUtil::recv(const ssl_ptr &ssl, void *dst, size_t size) const
    {
        int ret = SSL_read(ssl.get(), dst, size);
        return ret > 0 ? ret : handle_err(ssl.get(), ret);
    }

    int TLSUtil::connect(const ssl_ptr &ssl) const
    {
        int ret = SSL_connect(ssl.get());
        return ret > 0 ? ret : handle_err(ssl.get(), ret);
    }

    int TLSUtil::accept(const ssl_ptr &ssl) const
    {
        // if NIO,it could return -1 and SSL_ERROR_WANT_WRITE or SSL_ERROR_WANT_READ
        int ret = SSL_accept(ssl.get());
        return ret > 0 ? ret : handle_err(ssl.get(), ret);
    }

    bool TLSUtil::set_CA(const std::string &cert) const
    {
        int err = SSL_CTX_load_verify_locations(m_ctx, cert.c_str(), nullptr);
        if (1 != err)
        {
            ERR_print_errors_fp(stderr);
            return false;
        }
        return true;
    }

    void TLSUtil::set_if_verify_peer_crt(bool verify) const
    {
        int mode = verify ? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT : SSL_VERIFY_NONE;
        SSL_CTX_set_verify(m_ctx, mode, nullptr);
    }

    TLSUtil::ssl_ptr TLSUtil::get_ssl(uint32_t fd)
    {
        SSL *ssl = SSL_new(m_ctx);
        if (!ssl)
        {
            ERR_print_errors_fp(stderr);
            return ssl_ptr(nullptr);
        }
        // abvoe TLS1.2
        SSL_set_min_proto_version(ssl, TLS1_2_VERSION);
        SSL_set_fd(ssl, fd);
        return ssl_ptr(ssl, SSLDeleter());
    }

    bool TLSUtil::set_crt_key(const std::string &cert, const std::string &key, int file_type) const
    {
        int err = 0;
        err = SSL_CTX_use_certificate_file(m_ctx, cert.c_str(), file_type);
        err = SSL_CTX_use_PrivateKey_file(m_ctx, key.c_str(), file_type);
        err = SSL_CTX_check_private_key(m_ctx);
        if (1 != err)
        {
            ERR_print_errors_fp(stderr);
            return false;
        }
        return true;
    }

    void TLSUtil::init()
    {
        const SSL_METHOD *method = (m_is_server ? TLS_server_method() : TLS_client_method());
        m_ctx = SSL_CTX_new(method);
        if (!m_ctx)
        {
            ERR_print_errors_fp(stderr);
            return;
        }
    }

    void TLSUtil::cleanup()
    {
        if (m_ctx)
        {
            SSL_CTX_free(m_ctx);
            m_ctx = nullptr;
        }
    }

    TLSUtil::TLSUtil(bool is_server) : m_ctx(nullptr), m_is_server(is_server)
    {
        init();
    }

    TLSUtil::~TLSUtil()
    {
        cleanup();
    }

} // namespace soda
