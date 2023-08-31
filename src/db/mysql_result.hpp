#pragma once

// MySQL result set of SELECT

#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../general/util.hpp"

namespace soda
{
    class MySQLResult : Noncopyable
    {
    private:
        MYSQL_RES *m_res;
        MYSQL_FIELD *m_fields;

    public:
        size_t row_num() const;

        size_t col_num() const;

        char **row_at(size_t index) const;

        char **next_row() const;

        char *field_name_at(size_t index) const;

        // get size of filed at index in current row
        size_t field_size_at(size_t index) const;

        // get all size of fields in current row
        size_t *fields_size() const;

        MySQLResult(MYSQL_RES *res);

        MySQLResult();

        ~MySQLResult();

        friend std::ostream &operator<<(std::ostream &os, const MySQLResult &res)
        {
            size_t rows = res.row_num();
            size_t cols = res.col_num();
            os << "rows: " << rows << " cols: " << cols << "\n";
            os << " | ";
            for (size_t i = 0; i < cols; ++i)
            {
                os << res.field_name_at(i) << " | " << std::flush;
            }
            os << "\n";
            for (size_t i = 0; i < rows; ++i)
            {
                os << " | ";
                char **row = res.row_at(i);
                for (size_t j = 0; j < cols; ++j)
                {
                    os << row[j] << " | ";
                }
                os << "\n";
            }
            return os;
        }

    private:
        void init();
        void move_from(MySQLResult &rhs);
        void clear();
    };

    size_t MySQLResult::row_num() const
    {
        return mysql_num_rows(m_res);
    }

    size_t MySQLResult::col_num() const
    {
        return mysql_num_fields(m_res);
    }

    char *MySQLResult::field_name_at(size_t index) const
    {
        if (index >= mysql_num_fields(m_res))
        {
            return nullptr;
        }
        return m_fields[index].name;
    }

    char **MySQLResult::row_at(size_t index) const
    {
        if (index >= mysql_num_rows(m_res))
        {
            return nullptr;
        }

        mysql_data_seek(m_res, index);
        return mysql_fetch_row(m_res);
    }

    char **MySQLResult::next_row() const
    {
        return mysql_fetch_row(m_res);
    }

    // get size of filed at index in current row
    size_t MySQLResult::field_size_at(size_t index) const
    {
        if (index >= mysql_num_fields(m_res))
        {
            return 0;
        }
        return *(mysql_fetch_lengths(m_res) + index);
    }

    // get all size of fields in current row
    size_t *MySQLResult::fields_size() const
    {
        return mysql_fetch_lengths(m_res);
    }

    void MySQLResult::init()
    {
        if (m_res)
        {
            mysql_data_seek(m_res, 0);
            mysql_field_seek(m_res, 0);
            m_fields = mysql_fetch_fields(m_res);
        }
    }

    MySQLResult::MySQLResult(MYSQL_RES *res) : m_res(res), m_fields(nullptr)
    {
        init();
    }

    MySQLResult::MySQLResult() : m_res(nullptr), m_fields(nullptr) {}

    void MySQLResult::move_from(MySQLResult &rhs)
    {
        m_res = rhs.m_res;
        m_fields = rhs.m_fields;
        rhs.m_res = nullptr;
        rhs.m_fields = nullptr;
    }

    void MySQLResult::clear()
    {
        if (m_res)
        {
            mysql_free_result(m_res);
            m_res = nullptr;
            m_fields = nullptr;
        }
    }

    MySQLResult::~MySQLResult()
    {
        clear();
    }
} // namespace soda
