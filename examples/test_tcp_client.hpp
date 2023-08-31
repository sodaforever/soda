
#include <iostream>
#include "../src/network/tcp_client.hpp"

using namespace std;
using namespace soda;

void recv_cb(TCPClient &c, int32_t fd, const string &addr, uint16_t port, const void *data, size_t data_size)
{
    const char *str = (char *)data;
    string content(str, data_size);
    cout << "From - " << addr << ":" << port << "\n"
         << content << flush;
}

void conn_cb(TCPClient &c, const string &addr, uint16_t port)
{
    cout << addr << ":" << port << " connected " << endl;
}

void disconn_cb(TCPClient &c, const string &addr, uint16_t port)
{
    cout << addr << ":" << port << " disconnected " << endl;
}

int main(int argc, char *argv[])
{

    // string addr = argv[1];
    // uint16_t port = stoi(argv[2]);

    string addr = "host.docker.internal";
    uint16_t port = 9999;

    TCPClient c(addr, port);

    c.set_callback_on_recv(recv_cb);
    c.set_callback_on_conn(conn_cb);
    c.set_callback_on_disconn(disconn_cb);
    c.start();

    while (1)
    {
        string input;
        getline(cin, input);
        input += "\n";
        c.send(reinterpret_cast<const uint8_t *>(input.c_str()), input.size());
    }

    return 0;
}
