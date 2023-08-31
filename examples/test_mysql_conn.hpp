
#include <iostream>
#include <thread>
#include "../src/db/mysql_conn.hpp"

using namespace std;
using namespace soda;

int main()
{

    // MySQLConn c("host.docker.internal", "root", "1234", "testdb", 33061);
    // MySQLConn c("127.0.0.1", "root", "1234", "testdb", 33061);
    // MySQLConn c("host=host.docker.internal;port=33061;user=root;passwd=1234;dbname=testdb;");
    MySQLConn c;
    // c.set_conn_info("host=host.docker.internal;port=33061;user=client;passwd=client;dbname=testdb;");
    c.set_conn_info("host=host.docker.internal;port=6033;user=proxysql;passwd=proxysql;dbname=testdb;");

    c.connect();
    // string q = "insert into t1 (c1, c2, c3, c4, c5) values(123,'tom',9.99,'2023-10-10 10:10:10','testetstets' )";

    PRINT_WITH_DIVIDER("delete all");
    string q = "delete from t1";
    cout << "affected " << c.execute_wr(q) << " rows" << endl;
    q = "select * from t1";
    auto res = c.execute_rd(q);
    cout << *res << endl;

    PRINT_WITH_DIVIDER("insert one");

    q = "insert into t1 (c1, c2, c3, c4, c5) values(123,'tom',9.99,now(),'testetstets' )";
    cout << "affected " << c.execute_wr(q) << " rows" << endl;
    q = "select * from t1";
    res = c.execute_rd(q);
    cout << *res << endl;

    PRINT_WITH_DIVIDER("rollback");

    c.tx_begin();
    q = "delete from t1";
    cout << "affected " << c.execute_wr(q) << " rows" << endl;
    q = "insert into t1 (c1, c2, c3, c4, c5) values(123,'tom',9.99,now(),'testtesttest' )";
    cout << "affected " << c.execute_wr(q) << " rows" << endl;
    q = "insert into t1 (c1, c2, c3, c4, c5) values(123,'tom',9.99,now(),'testtesttest' )";
    cout << "affected " << c.execute_wr(q) << " rows" << endl;
    c.tx_rollback();

    q = "select * from t1";
    res = c.execute_rd(q);
    cout << *res << endl;

    PRINT_WITH_DIVIDER("commit");

    c.tx_begin();
    q = "delete from t1";
    cout << "affected " << c.execute_wr(q) << " rows" << endl;
    q = "insert into t1 (c1, c2, c3, c4, c5) values(123,'tom',9.99,now(),'testtesttest' )";
    cout << "affected " << c.execute_wr(q) << " rows" << endl;
    q = "insert into t1 (c1, c2, c3, c4, c5) values(123,'tom',9.99,now(),'testtesttest' )";
    cout << "affected " << c.execute_wr(q) << " rows" << endl;
    c.tx_commit();

    q = "select * from t1";
    res = c.execute_rd(q);
    cout << *res << endl;

    PRINT_WITH_DIVIDER("prepared_stmt update");

    q = "update t1 set c2 = 'Jerry', c3 = ? where c1 = ?";

    auto stmt = c.get_stmt(q.c_str());
    string p_str = "10.01";
    int64_t p_i = 123;
    stmt->bind_batch(0, p_str, p_i);
    // stmt->bind(0, p_str);
    // stmt->bind(1, p_i);
    cout << "affected " << stmt->execute_wr() << " rows" << endl;

    PRINT_WITH_DIVIDER("prepared_stmt execute_rd");

    q = "select * from t1 where c2 = ?";

    stmt = c.get_stmt(q.c_str());
    p_str = "Jerry";
    stmt->bind(0, p_str);

    cout << *(stmt->execute_rd()) << endl;

    PRINT_WITH_DIVIDER("prepared_stmt insert");

    q = "insert into t1 (c1,c2,c3,c4,c5) values(?,?,?,?,?)";
    stmt = c.get_stmt(q.c_str());
    uint16_t p_i16 = 456;
    stmt->bind(0, p_i16);
    stmt->bind(1, "Cathy", 5);
    stmt->bind(2, nullptr);
    stmt->bind(3, "2020-09-08 11:22:33", 19);
    stmt->bind(4, "hello cpp", 9);
    cout << "affected " << stmt->execute_wr() << " rows" << endl;

    PRINT_WITH_DIVIDER("prepared_stmt execute_rd");

    q = "select * from t1 where c2 = ?";

    stmt = c.get_stmt(q.c_str());
    p_str = "Cathy";
    stmt->bind(0, p_str);

    cout << *(stmt->execute_rd()) << endl;

    // for (size_t i = 0; i < stmt_res->row_num(); ++i)
    // {
    //     for (size_t j = 0; j < stmt_res->col_num(); ++j)
    //     {
    //         if (stmt_res->is_null(i, j))
    //         {
    //             cout << "null" << endl;
    //         }
    //         else
    //         {
    //             cout << stmt_res->execute_rd_string(i, j) << endl;
    //         }
    //     }
    // }

    // while (1)
    // {
    //     this_thread::sleep_for(chrono::seconds(2));
    //     cout << "..." << endl;
    // }

    return 0;
}
