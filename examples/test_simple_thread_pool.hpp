
#include <future>

#include "../src/thread/simple_thread_pool.hpp"

using namespace std;
using namespace soda;

int sum(int lhs, int rhs)
{
    cout << lhs + rhs << endl;
    return lhs + rhs;
}

int count()
{
    static int i = 0;
    ++i;
    cout << i << endl;

    return i;
}

int main()
{
    SimpleThreadPool tp(5);
    int r = 0;
    for (size_t i = 0; i < 100; i++)
    {
        auto ret = tp.insert_task(::count);

        r = sum(r, ret.get());
    }

    while (1)
    {
        this_thread::sleep_for(chrono::seconds(5));
        tp.stop();
    }

    while (1)
    {
        this_thread::sleep_for(chrono::seconds(5000));
    }

    return 0;
}