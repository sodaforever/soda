#pragma once

// MySQL result set of SELECT

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <deque>

#include "mysql_util.hpp"
#include "../general/util.hpp"

namespace soda
{
    class MySQLStmtResult : Noncopyable
    {
    private:
        size_t m_num_row;
        size_t m_num_col;
        std::vector<std::vector<uint8_t *>> m_value;
        std::vector<std::vector<size_t>> m_field_size;
        std::vector<std::deque<bool>> m_is_null_val;
        std::vector<std::deque<bool>> m_is_unsigned;
        std::vector<std::deque<bool>> m_error;

        MYSQL_STMT *m_stmt;
        MYSQL_RES *m_meta_res;
        MYSQL_BIND *m_res_bind;
        MYSQL_FIELD *m_meta_fields;

        char m_pre;

    public:
        MySQLStmtResult(MYSQL_STMT *stmt);
        ~MySQLStmtResult();

        size_t row_num() const;
        size_t col_num() const;

        const uint8_t *value(size_t row_idx, size_t col_idx) const;

        const char *field_name(size_t index) const;
        size_t field_size(size_t row_idx, size_t col_idx) const;

        bool is_null(size_t row_idx, size_t col_idx) const;
        bool is_unsigned(size_t row_idx, size_t col_idx) const;

        int64_t get_integer(size_t row_idx, size_t col_idx) const;
        uint64_t get_uinteger(size_t row_idx, size_t col_idx) const;
        double get_double(size_t row_idx, size_t col_idx) const;
        std::string get_string(size_t row_idx, size_t col_idx) const;
        std::string get_datetime(size_t row_idx, size_t col_idx) const;

        friend std::ostream &operator<<(std::ostream &os, const MySQLStmtResult &res)
        {
            size_t rows = res.row_num();
            size_t cols = res.col_num();
            os << "rows: " << rows << " cols: " << cols << "\n";
            os << " | ";
            for (size_t i = 0; i < cols; ++i)
            {
                os << res.field_name(i) << " | " << std::flush;
            }
            os << "\n";
            for (size_t i = 0; i < rows; ++i)
            {
                os << " | ";
                for (size_t j = 0; j < cols; ++j)
                {
                    os << res.get_string(i, j) << " | ";
                }
                os << "\n";
            }
            return os;
        }

    private:
        void clear();
        void init_res();
        void bind(size_t row_index);
        void fetch_row();
        void store_data();
    };

    void MySQLStmtResult::init_res()
    {
        if (0 != mysql_stmt_store_result(m_stmt))
        {
            ERROR_PRINT(mysql_stmt_error(m_stmt));
        }

        m_meta_res = mysql_stmt_result_metadata(m_stmt);
        if (!m_meta_res && (0 != mysql_stmt_errno(m_stmt)))
        {
            ERROR_PRINT(mysql_stmt_error(m_stmt));
            return;
        }

        m_meta_fields = mysql_fetch_fields(m_meta_res);

        m_num_col = mysql_num_fields(m_meta_res);
        m_num_row = mysql_stmt_num_rows(m_stmt);

        m_value.resize(m_num_row, std::vector<uint8_t *>(m_num_col, nullptr));
        m_field_size.resize(m_num_row, std::vector<size_t>(m_num_col, 0));
        m_is_null_val.resize(m_num_row, std::deque<bool>(m_num_col, false));
        m_is_unsigned.resize(m_num_row, std::deque<bool>(m_num_col, false));
        m_error.resize(m_num_row, std::deque<bool>(m_num_col, false));

        m_res_bind = new MYSQL_BIND[m_num_col]{};

        store_data();
        mysql_stmt_free_result(m_stmt);
    }

    void MySQLStmtResult::store_data()
    {
        for (size_t i = 0; i < m_num_row; ++i)
        {
            bind(i);
            fetch_row();
            memset(m_res_bind, 0, sizeof(MYSQL_BIND) * m_num_col);
        }
    }

    void MySQLStmtResult::bind(size_t row_index)
    {
        memset(m_res_bind, 0, sizeof(MYSQL_BIND) * m_num_col);
        for (size_t i = 0; i < m_num_col; i++)
        {
            m_res_bind[i].buffer_type = get_field_type(m_meta_fields + i);
            m_res_bind[i].length = &m_field_size[row_index][i];
            m_res_bind[i].is_null = &m_is_null_val[row_index][i];
            m_res_bind[i].error = &m_error[row_index][i];
            m_res_bind[i].buffer_length = get_field_size(m_meta_fields + i);
            uint8_t *value = new uint8_t[m_res_bind[i].buffer_length]{};
            m_res_bind[i].buffer = value;
            m_value[row_index][i] = value;
        }
        if (0 != mysql_stmt_bind_result(m_stmt, m_res_bind))
        {
            ERROR_PRINT(mysql_stmt_error(m_stmt));
            return;
        }
    }

    void MySQLStmtResult::fetch_row()
    {
        if (0 != mysql_stmt_fetch(m_stmt))
        {
            ERROR_PRINT(mysql_stmt_error(m_stmt));
            return;
        }
    }

    std::string MySQLStmtResult::get_datetime(size_t row_idx, size_t col_idx) const
    {
        return get_string(row_idx, col_idx);
    }

    std::string MySQLStmtResult::get_string(size_t row_idx, size_t col_idx) const
    {
        if (row_idx >= m_num_row || col_idx >= m_num_col || is_null(row_idx, col_idx))
        {
            return "";
        }
        if (is_null(row_idx, col_idx))
        {
            return "";
        }

        std::ostringstream os;
        void *val = m_value[row_idx][col_idx];

        switch (get_field_type(m_meta_fields + col_idx))
        {
        case MYSQL_TYPE_TINY:
            os << *reinterpret_cast<int8_t *>(val);
            break;

        case MYSQL_TYPE_SHORT:
            os << *reinterpret_cast<int16_t *>(val);
            break;

        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
            os << *reinterpret_cast<int32_t *>(val);
            break;

        case MYSQL_TYPE_LONGLONG:
            os << *reinterpret_cast<int64_t *>(val);
            break;

        case MYSQL_TYPE_FLOAT:
            os << *reinterpret_cast<float *>(val);
            break;

        case MYSQL_TYPE_DOUBLE:
            os << *reinterpret_cast<double *>(val);
            break;

        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_JSON:
            os << reinterpret_cast<char *>(val);
            break;

        case MYSQL_TYPE_YEAR:
            os << static_cast<unsigned int>(*reinterpret_cast<uint8_t *>(val));
            break;

        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
        {
            MYSQL_TIME *time = reinterpret_cast<MYSQL_TIME *>(val);
            os << time->year << "-" << time->month << "-" << time->day
               << " " << time->hour << ":" << time->minute << ":" << time->second;
            break;
        }

        default:
            os << "";
            break;
        }

        return os.str();
    }

    double MySQLStmtResult::get_double(size_t row_idx, size_t col_idx) const
    {
        if (row_idx >= m_num_row || col_idx >= m_num_col || is_null(row_idx, col_idx))
        {
            return 0.0;
        }
        if (is_null(row_idx, col_idx))
        {
            return 0.0;
        }

        void *val = m_value[row_idx][col_idx];

        switch (get_field_type(m_meta_fields + col_idx))
        {

        case MYSQL_TYPE_TINY:
            return *reinterpret_cast<int8_t *>(val);

        case MYSQL_TYPE_SHORT:
            return *reinterpret_cast<int16_t *>(val);

        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
            return *reinterpret_cast<int32_t *>(val);

        case MYSQL_TYPE_LONGLONG:
            return static_cast<double>(*reinterpret_cast<int64_t *>(val));

        case MYSQL_TYPE_FLOAT:
            return *reinterpret_cast<float *>(val);

        case MYSQL_TYPE_DOUBLE:
            return *reinterpret_cast<double *>(val);

        case MYSQL_TYPE_NEWDECIMAL:
            return std::stod(reinterpret_cast<char *>(val));

        case MYSQL_TYPE_YEAR:
            return *reinterpret_cast<uint8_t *>(val);

        default:
            return 0.0;
        }
    }

    int64_t
    MySQLStmtResult::get_integer(size_t row_idx, size_t col_idx) const
    {
        if (row_idx >= m_num_row || col_idx >= m_num_col || is_null(row_idx, col_idx))
        {
            return 0;
        }
        if (is_null(row_idx, col_idx))
        {
            return 0;
        }

        void *val = m_value[row_idx][col_idx];

        switch (get_field_type(m_meta_fields + col_idx))
        {
        case MYSQL_TYPE_TINY:
            return *reinterpret_cast<int8_t *>(val);
        case MYSQL_TYPE_SHORT:
            return *reinterpret_cast<int16_t *>(val);
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
            return *reinterpret_cast<int32_t *>(val);
        case MYSQL_TYPE_LONGLONG:
            return *reinterpret_cast<int64_t *>(val);
        case MYSQL_TYPE_FLOAT:
            return static_cast<int64_t>(*reinterpret_cast<float *>(val));
        case MYSQL_TYPE_DOUBLE:
            return static_cast<int64_t>(*reinterpret_cast<double *>(val));
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VAR_STRING:
            return std::stoull(reinterpret_cast<char *>(val));
        case MYSQL_TYPE_YEAR:
            return *reinterpret_cast<uint8_t *>(val);
        default:
            return 0;
        }
    }

    uint64_t MySQLStmtResult::get_uinteger(size_t row_idx, size_t col_idx) const
    {
        if (row_idx >= m_num_row || col_idx >= m_num_col || is_null(row_idx, col_idx))
        {
            return 0;
        }
        if (is_null(row_idx, col_idx))
        {
            return 0;
        }

        void *val = m_value[row_idx][col_idx];

        switch (get_field_type(m_meta_fields + col_idx))
        {
        case MYSQL_TYPE_TINY:
            return *reinterpret_cast<uint8_t *>(val);
        case MYSQL_TYPE_SHORT:
            return *reinterpret_cast<uint16_t *>(val);
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
            return *reinterpret_cast<uint32_t *>(val);
        case MYSQL_TYPE_LONGLONG:
            return *reinterpret_cast<uint64_t *>(val);
        case MYSQL_TYPE_FLOAT:
            return static_cast<uint64_t>(*reinterpret_cast<float *>(val));
        case MYSQL_TYPE_DOUBLE:
            return static_cast<uint64_t>(*reinterpret_cast<double *>(val));
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VAR_STRING:
            return std::stoull(std::string(reinterpret_cast<char *>(val)));
        case MYSQL_TYPE_YEAR:
            return *reinterpret_cast<uint8_t *>(val);
        default:
            return 0;
        }
    }

    bool MySQLStmtResult::is_unsigned(size_t row_idx, size_t col_idx) const
    {
        if (row_idx >= m_num_row || col_idx >= m_num_col)
        {
            return true;
        }
        return m_is_unsigned[row_idx][col_idx];
    }

    bool MySQLStmtResult::is_null(size_t row_idx, size_t col_idx) const
    {
        if (row_idx >= m_num_row || col_idx >= m_num_col)
        {
            return true;
        }
        return m_is_null_val[row_idx][col_idx];
    }

    size_t MySQLStmtResult::row_num() const
    {
        return m_num_row;
    }

    size_t MySQLStmtResult::col_num() const
    {
        return m_num_col;
    }

    const char *MySQLStmtResult::field_name(size_t index) const
    {
        if (index >= m_num_col)
        {
            return nullptr;
        }
        return get_field_name(m_meta_fields + index);
    }

    const uint8_t *MySQLStmtResult::value(size_t row_idx, size_t col_idx) const
    {
        if (row_idx >= m_num_row || col_idx >= m_num_col)
        {
            return nullptr;
        }

        return m_value[row_idx][col_idx];
    }

    size_t MySQLStmtResult::field_size(size_t row_idx, size_t col_idx) const
    {
        if (row_idx >= m_num_row || col_idx >= m_num_col)
        {
            return 0;
        }

        return m_field_size[row_idx][col_idx];
    }

    MySQLStmtResult::MySQLStmtResult(MYSQL_STMT *stmt) : m_num_row(0), m_num_col(0), m_stmt(stmt), m_meta_res(nullptr), m_res_bind(nullptr)
    {
        init_res();
    }

    void MySQLStmtResult::clear()
    {
        if (!m_value.empty())
        {
            for (size_t i = 0; i < m_value.size(); ++i)
            {
                for (size_t j = 0; j < m_value[i].size(); j++)
                {
                    delete[] m_value[i][j];
                    m_value[i][j] = nullptr;
                }
            }
        }
        m_value.clear();
        m_field_size.clear();
        m_is_null_val.clear();
        m_is_unsigned.clear();
        m_error.clear();
        m_num_col = 0;
        m_num_row = 0;
        if (m_meta_res)
        {
            mysql_free_result(m_meta_res);
            m_meta_res = nullptr;
        }
        if (m_res_bind)
        {
            delete[] m_res_bind;
            m_res_bind = nullptr;
        }
    }

    MySQLStmtResult::~MySQLStmtResult()
    {
        clear();
    }
} // namespace soda
