
#include <iostream>
#include "../src/buffer/atomic_ring_buffer.hpp"

using namespace std;
using namespace soda;

int main()
{
    using namespace std;

    soda::AtomicRingBuffer rb{5};

    for (int i = 0; i < 10; ++i)
    {
        rb.write(&i, 1);
        cout << rb << endl;
    }

    int r = 0;
    for (int i = 0; i < 10; ++i)
    {
        rb.read(&r, 1);
        cout << rb << endl;
        cout << r << endl;
    }

    int arr[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    rb.write(arr, 5);
    cout << rb << endl;
    int r_arr[10];
    rb.read(r_arr, 2);
    cout << rb << endl;
    rb.write(arr + 5, 5);
    cout << rb << endl;
    rb.read(r_arr, 5);
    cout << rb << endl;
    rb.clear();
    cout << rb << endl;
    rb.write(arr, 5);
    cout << rb << endl;
    return 0;
}