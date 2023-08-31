
#include <iostream>
#include "../src/network/udp_server.hpp"

using namespace std;
using namespace soda;

void recv_cb(UDPServer &s,
             int32_t fd,
             const string &addr,
             uint16_t port,
             const void *data,
             size_t data_size)
{
    const char *str = (char *)data;
    string content(str, data_size);
    cout << "From - " << addr << ":" << port << "\n"
         << content << flush;
}

int main(int argc, char *argv[])
{

    string addr = argv[1];
    uint16_t port = stoi(argv[2]);

    string c_addr = argv[3];
    uint16_t c_port = stoi(argv[4]);

    // host.docker.internal
    UDPServer c(addr, port);

    // UDPServer c("127.0.0.1", 10002);
    c.set_callback_on_recv(recv_cb);
    c.start();

    while (1)
    {
        string input;
        getline(cin, input);
        input += "\n";
        c.send(reinterpret_cast<const uint8_t *>(input.c_str()), input.size(), c_addr, c_port);
        // const uint8_t *ic = reinterpret_cast<const uint8_t *>(input.c_str());
        // c.send(ic, input.size(), "127.0.0.1", 10001);
    }

    return 0;
}
