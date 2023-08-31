#pragma once

// database connection base class

#include <string>
#include <unordered_map>
#include <sstream>

#include "../general/util.hpp"

namespace soda
{
    class ConnBase : Noncopyable
    {
    protected:
        //"host=127.0.0.1;port=3306;user=dbuser;passwd=dbpasswd;dbname=mydb;"
        std::string m_conn_str;
        std::unordered_map<std::string, std::string> m_conn_info;

    public:
        explicit ConnBase(const std::string &conn_str);
        ConnBase() {}
        virtual ~ConnBase();

        void set_conn_info(const std::string &conn_str);

        virtual bool connect() = 0;

        virtual void close() = 0;

        virtual bool ping() = 0;

    private:
        void parse_conn_info();
    };

    void ConnBase::parse_conn_info()
    {
        std::istringstream ss(m_conn_str);
        std::string token;
        while (std::getline(ss, token, ';'))
        {
            size_t pos = token.find('=');
            if (pos != std::string::npos)
            {
                m_conn_info[token.substr(0, pos)] = token.substr(pos + 1);
            }
        }
    }

    void ConnBase::set_conn_info(const std::string &conn_str)
    {
        m_conn_str = conn_str;
        parse_conn_info();
    }

    ConnBase::ConnBase(const std::string &conn_str) : m_conn_str(conn_str)
    {
        parse_conn_info();
    }

    ConnBase::~ConnBase() {}

} // namespace soda
