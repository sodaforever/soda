#pragma once

// MySQL Connector
// conn_str "host=127.0.0.1;port=3306;user=dbuser;passwd=dbpasswd;dbname=mydb;usock=0;cflag=0;"

#include <memory>

#include "conn_base.hpp"
#include "mysql_result.hpp"
#include "mysql_stmt.hpp"

namespace soda
{
    class MySQLInitializer
    {
    public:
        MySQLInitializer() { mysql_library_init(0, nullptr, nullptr); }
        ~MySQLInitializer() { mysql_library_end(); }
    };

    class MySQLConn : public ConnBase
    {
    private:
        std::string m_host;
        std::string m_user;
        std::string m_passwd;
        std::string m_dbname;
        uint16_t m_port;
        const char *m_usock;
        uint64_t m_cflag;
        MYSQL *m_conn;
        bool m_connected;
        std::unordered_map<std::string, std::shared_ptr<MySQLStmt>> m_stmts;

    public:
        MySQLConn(const std::string &host, const std::string &user, const std::string &passwd,
                  const std::string &dbname, uint16_t port = 3306, const char *unix_socket = nullptr,
                  unsigned long client_flag = 0);
        MySQLConn(const std::string &conn_str);
        MySQLConn();
        ~MySQLConn();

        bool connect() override;
        void close() override;

        bool ping() override;

        // for update/insert/delete...
        // -1 if failed; num of affected rows if done
        int execute_wr(const std::string &cmd);
        // for select/explain...
        // nullptr if failed
        std::shared_ptr<MySQLResult> execute_rd(const std::string &cmd);

        void tx_begin();
        void tx_commit();
        void tx_rollback();

        std::shared_ptr<MySQLStmt> get_stmt(const std::string &cmd);
    };

    std::shared_ptr<MySQLStmt> MySQLConn::get_stmt(const std::string &cmd)
    {
        if (m_stmts.find(cmd) != m_stmts.end())
        {
            return m_stmts.at(cmd);
        }
        auto stmt_ptr = std::make_shared<MySQLStmt>(m_conn, cmd);
        m_stmts[cmd] = stmt_ptr;
        return stmt_ptr;
    }

    void MySQLConn::tx_rollback()
    {
        mysql_query(m_conn, "ROLLBACK");
        mysql_autocommit(m_conn, 1);
    }

    void MySQLConn::tx_commit()
    {
        mysql_query(m_conn, "COMMIT");
        mysql_autocommit(m_conn, 1);
    }

    void MySQLConn::tx_begin()
    {
        mysql_autocommit(m_conn, 0);
        mysql_query(m_conn, "START TRANSACTION");
    }

    // for select/explain...
    // nullptr if failed
    std::shared_ptr<MySQLResult> MySQLConn::execute_rd(const std::string &cmd)
    {
        if (0 != mysql_query(m_conn, cmd.c_str()))
        {
            ERROR_PRINT(mysql_error(m_conn));
            return nullptr;
        }

        MYSQL_RES *res = mysql_store_result(m_conn);
        if (!res)
        {
            ERROR_PRINT(mysql_error(m_conn));
            return nullptr;
        }

        return std::make_shared<MySQLResult>(res);
    }

    // for update/insert/delete...
    // -1 if failed; num of affected rows if done
    int MySQLConn::execute_wr(const std::string &cmd)
    {
        if (0 != mysql_query(m_conn, cmd.c_str()))
        {
            ERROR_PRINT(mysql_error(m_conn));
            return -1;
        }
        return mysql_affected_rows(m_conn);
    }

    void MySQLConn::close()
    {
        if (m_conn)
        {
            mysql_close(m_conn);
            m_conn = nullptr;
        }

        m_stmts.clear();
    }

    bool MySQLConn::connect()
    {
        if (!m_conn)
        {
            ERROR_PRINT("Connect fialed, need init");
        }
        else if (m_connected)
        {
            ERROR_PRINT("Connect fialed, has connected");
        }
        else
        {
            if (nullptr != mysql_real_connect(m_conn,
                                              m_conn_info.at("host").c_str(),
                                              m_conn_info.at("user").c_str(),
                                              m_conn_info.at("passwd").c_str(),
                                              m_conn_info.at("dbname").c_str(),
                                              std::stoi(m_conn_info.at("port").c_str()),
                                              m_conn_info.find("usock") == m_conn_info.end() ? nullptr : m_conn_info.at("usock").c_str(),
                                              m_conn_info.find("cflag") == m_conn_info.end() ? 0 : std::stoul(m_conn_info.at("cflag").c_str())))
            {
                m_connected = true;
                return true;
            }

            ERROR_PRINT(mysql_error(m_conn));
        }
        return false;
    }
    bool MySQLConn::ping()
    {
        if (!m_connected)
        {
            return false;
        }
        if (0 != mysql_query(m_conn, "SELECT 1"))
        {
            DEBUG_PRINT(mysql_error(m_conn));
            unsigned int error_code = mysql_errno(m_conn);
            if (error_code == CR_SERVER_GONE_ERROR || error_code == CR_SERVER_LOST)
            {
                m_connected = false;
                return false;
            }
        }
        MYSQL_RES *res = mysql_store_result(m_conn);
        if (!res)
        {
            DEBUG_PRINT(mysql_error(m_conn));
            unsigned int error_code = mysql_errno(m_conn);
            if (error_code == CR_SERVER_GONE_ERROR || error_code == CR_SERVER_LOST)
            {
                m_connected = false;
                return false;
            }
        }
        mysql_free_result(res);
        return true;
    }

    MySQLConn::MySQLConn(const std::string &host,
                         const std::string &user,
                         const std::string &passwd,
                         const std::string &dbname,
                         uint16_t port,
                         const char *unix_socket,
                         unsigned long client_flag) : ConnBase(""),
                                                      m_host(host),
                                                      m_user(user),
                                                      m_passwd(passwd),
                                                      m_dbname(dbname),
                                                      m_port(port),
                                                      m_usock(unix_socket),
                                                      m_cflag(client_flag),
                                                      m_connected(false)
    {
        static MySQLInitializer mysql_initializer;

        m_conn = mysql_init(nullptr);
        if (!m_conn)
        {
            ERROR_PRINT(mysql_error(m_conn));
        }
    }

    MySQLConn::MySQLConn(const std::string &conn_str) : ConnBase(conn_str), m_connected(false)
    {
        static MySQLInitializer mysql_initializer;

        m_conn = mysql_init(nullptr);
        if (!m_conn)
        {
            ERROR_PRINT(mysql_error(m_conn));
        }
    }

    MySQLConn::MySQLConn() : m_connected(false)
    {
        static MySQLInitializer mysql_initializer;

        m_conn = mysql_init(nullptr);
        if (!m_conn)
        {
            ERROR_PRINT(mysql_error(m_conn));
        }
    }

    MySQLConn::~MySQLConn()
    {
        DEBUG_PRINT("~MySQLConn");
        close();
    }

} // namespace soda
