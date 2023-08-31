#include "../src/db/conn_pool.hpp"
#include "../src/db/mysql_conn.hpp"
#include "../src/general/random.hpp"

using namespace std;
using namespace soda;

atomic_size_t count{0};

void sel(MySQLConn *c)
{
    string q = "select * from t1";
    auto res = c->execute_rd(q);
    cout << *res << endl;
    string output = to_string(++count) + " select " + " get " + to_string(res->row_num()) + " rows";
    PRINT_WITH_DIVIDER(output);
}

void update(MySQLConn *c)
{
    string q = "update t1 set c2 = ?, c3 = ?, c4 = now(),c5 = ? where c1 = 123";

    auto stmt = c->get_stmt(q.c_str());

    string c2 = random::get_str(5);
    double c3 = random::get_real(-100.0, 100.0);
    string c5 = random::get_str(10);
    stmt->bind_batch(0, c2, c3, c5);
    string output = to_string(++count) + " update " + " affected " + to_string(stmt->execute_wr()) + " rows";
    PRINT_WITH_DIVIDER(output);
}

void insert(MySQLConn *c)
{
    DEBUG_PRINT("insert");
    string q = "insert t1 (c1,c2,c3,c4,c5) values (?,?,?,now(),?)";

    auto stmt = c->get_stmt(q.c_str());
    size_t c1 = 123;
    string c2 = random::get_str(5);
    double c3 = random::get_real(-100.0, 100.0);
    string c5 = random::get_str(10);
    stmt->bind_batch(0, c1, c2, c3, c5);
    string output = to_string(++count) + " insert " + " affected " + to_string(stmt->execute_wr()) + " rows";
    PRINT_WITH_DIVIDER(output);
}

void del(MySQLConn *c)
{
    DEBUG_PRINT("delete");
    string q = "delete from t1";
    auto stmt = c->get_stmt(q.c_str());
    string output = to_string(++count) + " del " + " affected " + to_string(stmt->execute_wr()) + " rows";
    PRINT_WITH_DIVIDER(output);
}

vector<function<void(MySQLConn *)>> funcs{sel, update, insert, del};

void thread_exe(ConnPool<MySQLConn> *cp)
{

    while (count < 100)
    {
        this_thread::sleep_for(chrono::seconds(3));
        shared_ptr<MySQLConn> c = cp->acquire();
        funcs[random::get_int(0, 3)](c.get());
        cp->release(move(c));
    }
}

int main()
{
    string cs = "host=host.docker.internal;port=6033;user=proxysql;passwd=proxysql;dbname=testdb;";

    ConnPool<MySQLConn> conn_pool(cs, 1, 4);
    for (size_t i = 0; i < 8; i++)
    {
        thread t(thread_exe, &conn_pool);
        t.detach();
        // this_thread::sleep_for(chrono::milliseconds(300));
    }
    while (count < 100)
    {
        cout << conn_pool << endl;
        this_thread::sleep_for(chrono::seconds(1));
    }
}