
#include <iostream>
#include <cstring>
#include "../src/db/mysql_conn.hpp"

using namespace std;
using namespace soda;

int main(int argc, char *argv[])
{

    MYSQL *conn = mysql_init(nullptr);
    mysql_real_connect(conn, "host.docker.internal", "root", "1234", "testdb", 33061, nullptr, 0);
    string q = "delete from t1";
    mysql_query(conn, q.c_str());
    cout << "affected " << mysql_affected_rows(conn) << " rows" << endl;
    q = "insert into t1 (c1,c2,c3,c4,c5) values(123,'tom',9.99,now(),'testetstets')";
    mysql_query(conn, q.c_str());
    cout << "affected " << mysql_affected_rows(conn) << " rows" << endl;

    q = "select * from t1";
    mysql_query(conn, q.c_str());
    MYSQL_RES *res = mysql_store_result(conn);
    int rows = mysql_num_rows(res);
    int cols = mysql_num_fields(res);

    MYSQL_FIELD *f_name = mysql_fetch_fields(res);
    for (size_t i = 0; i < cols; i++)
    {
        cout << f_name[i].name << " | ";
    }
    cout << endl;

    char **row = mysql_fetch_row(res);
    for (; row; row = mysql_fetch_row(res))
    {
        for (size_t i = 0; i < cols; i++)
        {
            cout << row[i] << " | ";
        }
    }
    cout << endl;

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    q = "insert into t2 (c1,c2) values (?,?)";
    mysql_stmt_prepare(stmt, q.c_str(), q.size());
    MYSQL_BIND bind[2]{0};
    int p1 = 1;
    bind[0].buffer = &p1;
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    char p2[] = "hello";
    size_t p2_len = strlen(p2);
    bind[1].buffer = p2;
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].length = &p2_len;
    mysql_stmt_bind_param(stmt, bind);

    mysql_stmt_execute(stmt);
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);

    q = "select * from t2 where c1 = ?";
    // stmt = mysql_stmt_init(conn);
    // mysql_stmt_prepare(stmt, q.c_str(), q.size());
    // MYSQL_RES *meta_res = mysql_stmt_result_metadata(stmt);
    // MYSQL_FIELD *fields = mysql_fetch_fields(meta_res);
    stmt = mysql_stmt_init(conn);
    if (!stmt)
    {
        fprintf(stderr, "mysql_stmt_init() failed\n");
        exit(1); // Or other error handling
    }

    if (mysql_stmt_prepare(stmt, q.c_str(), q.size()))
    {
        fprintf(stderr, "mysql_stmt_prepare() failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        exit(1); // Or other error handling
    }

    MYSQL_RES *meta_res = mysql_stmt_result_metadata(stmt);
    if (!meta_res)
    {
        fprintf(stderr, "mysql_stmt_result_metadata() failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        exit(1); // Or other error handling
    }

    MYSQL_FIELD *fields = mysql_fetch_fields(meta_res);
    if (!fields)
    {
        fprintf(stderr, "mysql_fetch_fields() failed\n");
        mysql_free_result(meta_res);
        mysql_stmt_close(stmt);
        exit(1); // Or other error handling
    }

    int ret_cols = mysql_num_fields(meta_res);
    MYSQL_BIND *ret_b = new MYSQL_BIND[ret_cols]{};
    for (size_t i = 0; i < ret_cols; i++)
    {

        ret_b[i].buffer_type = fields[i].type;
        ret_b[i].buffer_length = fields[i].length + 1;
        ret_b[i].buffer = new char[ret_b[i].buffer_length]{0};
        ret_b[i].length = &ret_b[i].buffer_length;
        ret_b[i].is_null = 0;
    }

    mysql_stmt_bind_result(stmt, ret_b);

    MYSQL_BIND para_b[1]{};
    int id = 1;
    para_b[0].buffer = &id;
    para_b[0].buffer_type = MYSQL_TYPE_LONG;
    mysql_stmt_bind_param(stmt, para_b);

    mysql_stmt_execute(stmt);
    mysql_stmt_store_result(stmt);

    while (!mysql_stmt_fetch(stmt))
    {
        for (size_t i = 0; i < ret_cols; i++)
        {
            cout << *(int *)(ret_b[i].buffer) << " | ";
        }
    }

    for (size_t i = 0; i < ret_cols; i++)
    {
        delete static_cast<char *>(ret_b[i].buffer);
    }

    mysql_free_result(meta_res);
    mysql_stmt_free_result(stmt);

    mysql_stmt_close(stmt);

    // while (1)
    // {
    //     this_thread::sleep_for(chrono::seconds(2));
    //     cout << "..." << endl;
    // }

    return 0;
}
