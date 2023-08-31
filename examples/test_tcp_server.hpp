
#include <iostream>
#include "../src/general/random.hpp"
#include "../src/network/tcp_server.hpp"

using namespace std;
using namespace soda;

TCPServer s(10001);

void recv_cb(uint32_t fd, const string &ip, uint16_t port, const void *data, size_t data_size)
{
    const char *str = (char *)data;
    string content(str, data_size);
    cout << ip << ":" << port << " --- " << content << endl;
    string ret("response: ");
    ret += content += " ";
    ret += random::get_str(300);
    s.send(fd, reinterpret_cast<const uint8_t *>(ret.c_str()), ret.size());
}

void conn_cb(const string &ip, uint16_t port)
{
    cout << ip << ":" << port << " connected " << endl;
}

void disconn_cb(const string &ip, uint16_t port)
{
    cout << ip << ":" << port << " disconnected " << endl;
}

int main()
{

    s.set_callback_on_recv(recv_cb);
    s.set_callback_on_conn(conn_cb);
    s.set_callback_on_disconn(disconn_cb);
    s.start(5);

    while (1)
    {
        cout << s << endl;
        this_thread::sleep_for(chrono::seconds(5));
    }

    return 0;
}