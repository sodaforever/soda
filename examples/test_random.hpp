
#include <iostream>
#include "../src/general/random.hpp"

using namespace std;
using namespace soda;

int main()
{
    for (size_t i = 0; i < 100; i++)
    {
        cout << random::get_int(-100, +100) << endl;
    }
    for (size_t i = 0; i < 100; i++)
    {
        cout << random::get_real(-100, 100) << endl;
    }

    cout << random::get_str(5) << endl;

    return 0;
}