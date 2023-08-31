// #pragma once

// MySQL prepared statement

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <cstring>
#include <memory>

#include "../general/util.hpp"
#include "mysql_stmt_result.hpp"

namespace soda
{
    class MySQLStmt : Noncopyable
    {
    private:
        MYSQL_STMT *m_stmt;
        MYSQL_BIND *m_param_bind;
        size_t m_num_param;
        size_t m_num_res_cols;

    public:
        MySQLStmt(MYSQL *conn, const std::string &cmd);
        ~MySQLStmt();

        // blob
        void bind(size_t index, const uint8_t *data, size_t size);
        // char*
        void bind(size_t index, const char *data, size_t size);
        // Non-string
        template <typename T>
        enable_if_t<!std::is_same<rm_cvref_t<T>, std::string>::value>
        bind(size_t index, T &&data);
        // string blob datetime decimal ...
        template <typename T>
        enable_if_t<std::is_same<rm_cvref_t<T>, std::string>::value>
        bind(size_t index, T &&data);

        // bind_batch index is the start pos
        template <typename T, typename... Args>
        void bind_batch(size_t index, T &&data, Args &&...args);
        void bind_batch(size_t) {}

        // for update/insert/delete...
        // -1 if failed; num of affected rows if done
        int execute_wr();
        // for select/explain...
        // bullptr if failed
        std::shared_ptr<MySQLStmtResult> execute_rd();

    private:
        void init_param_bind();
        void clear_param_bind();

        void execute();

        void init(MYSQL *conn, const std::string &cmd);
        void clear();

        void bind(size_t index, enum_field_types type, const void *data, size_t size, bool is_unsigned);
    };

    void MySQLStmt::execute()
    {
        // bind each time befor execute
        if (0 != mysql_stmt_bind_param(m_stmt, m_param_bind))
        {
            ERROR_PRINT(mysql_stmt_error(m_stmt));
        }

        if (0 != mysql_stmt_execute(m_stmt))
        {
            ERROR_PRINT(mysql_stmt_error(m_stmt));
        }

        clear_param_bind();
    }

    void MySQLStmt::init(MYSQL *conn, const std::string &cmd)
    {
        m_stmt = mysql_stmt_init(conn);
        if (!m_stmt)
        {
            ERROR_PRINT(mysql_error(conn));
            return;
        }
        if (0 != mysql_stmt_prepare(m_stmt, cmd.c_str(), cmd.size()))
        {
            ERROR_PRINT(mysql_stmt_error(m_stmt));
            clear();
            return;
        }
        bool opt = 1;
        mysql_stmt_attr_set(m_stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &opt);

        init_param_bind();
    }

    void MySQLStmt::init_param_bind()
    {
        m_num_param = mysql_stmt_param_count(m_stmt);

        m_param_bind = new MYSQL_BIND[m_num_param]{};
    }

    void MySQLStmt::clear_param_bind()
    {
        if (m_param_bind)
        {
            for (size_t i = 0; i < m_num_param; ++i)
            {
                delete[] static_cast<uint8_t *>(m_param_bind[i].buffer);
                m_param_bind[i].buffer = nullptr;
            }
        }
    }

    void MySQLStmt::clear()
    {
        if (m_param_bind)
        {
            delete[] m_param_bind;
            m_param_bind = nullptr;
        }

        if (m_stmt)
        {
            mysql_stmt_close(m_stmt);
            m_stmt = nullptr;
        }
    }

    // for select/explain...
    // nullptr if failed
    std::shared_ptr<MySQLStmtResult> MySQLStmt::execute_rd()
    {
        execute();
        return std::make_shared<MySQLStmtResult>(m_stmt);
    }

    // for update/insert/delete...
    // -1 if failed; num of affected rows if done
    int MySQLStmt::execute_wr()
    {
        execute();
        return mysql_stmt_affected_rows(m_stmt);
    }

    // non-string
    template <typename T>
    enable_if_t<!std::is_same<rm_cvref_t<T>, std::string>::value>
    MySQLStmt::bind(size_t index, T &&data)
    {
        bind(index, MySQLTypeInfoUni<T>::value, &data, sizeof(T), MySQLTypeInfoUni<T>::is_unsigned);
    }

    // string blob datetime decimal ...
    template <typename T>
    enable_if_t<std::is_same<rm_cvref_t<T>, std::string>::value>
    MySQLStmt::bind(size_t index, T &&data)
    {
        bind(index, MySQLTypeInfoUni<T>::value, data.c_str(), data.size(), false);
    }

    // char*
    void MySQLStmt::bind(size_t index, const char *data, size_t size)
    {
        bind(index, MYSQL_TYPE_STRING, data, size, false);
    }

    // blob
    void MySQLStmt::bind(size_t index, const uint8_t *data, size_t size)
    {
        bind(index, MYSQL_TYPE_BLOB, data, size, false);
    }

    // bind_batch index is the start pos
    template <typename T, typename... Args>
    void MySQLStmt::bind_batch(size_t index, T &&data, Args &&...args)
    {
        bind(index, std::forward<T>(data));
        bind_batch(index + 1, std::forward<Args>(args)...);
    }

    void MySQLStmt::bind(size_t index, enum_field_types type, const void *data, size_t size, bool is_unsigned)
    {
        if (index >= m_num_param)
        {
            return;
        }
        m_param_bind[index].buffer_type = type;
        if (type != MYSQL_TYPE_NULL)
        {
            m_param_bind[index].buffer = new uint8_t[size]{};
            memcpy(m_param_bind[index].buffer, data, size);
            m_param_bind[index].buffer_length = size;
            m_param_bind[index].length = &m_param_bind[index].buffer_length;
        }
        m_param_bind[index].is_unsigned = is_unsigned;
    }

    MySQLStmt::~MySQLStmt()
    {
        DEBUG_PRINT("~MySQLStmt");
        clear();
    }

    MySQLStmt::MySQLStmt(MYSQL *conn, const std::string &cmd) : m_stmt(nullptr), m_param_bind(nullptr), m_num_param(0), m_num_res_cols(0)
    {
        init(conn, cmd);
    }
} // namespace soda