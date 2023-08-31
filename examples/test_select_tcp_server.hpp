
#include "../src/general/random.hpp"
#include "../src/network/select_tcp_server.hpp"

using namespace std;
using namespace soda;

void recv_cb(SelectTCPServer &s, int32_t fd, const string &addr, uint16_t port, const void *data, size_t data_size)
{
    const char *str = (char *)data;
    string content(str, data_size);
    cout << "From - " << addr << ":" << port << " fd:" << fd << "\n"
         << content << flush;
    string ret = "Recv - " + content;
    s.send(fd, reinterpret_cast<const uint8_t *>(ret.c_str()), ret.size());
}

void conn_cb(SelectTCPServer &s, int32_t fd, const string &addr, uint16_t port)
{
    cout << addr << ":" << port << " fd:" << fd << " connected " << endl;
}

void disconn_cb(SelectTCPServer &s, const string &addr, uint16_t port)
{
    cout << addr << ":" << port << " disconnected " << endl;
}

void send_msg(SelectTCPServer &s)
{
    string input;
    int32_t fd = -1;
    while (1)
    {
        getline(cin, input);
        if (input == "close")
        {
            cout << "Enter fd:" << endl;
            int32_t fd;
            cin >> fd;
            s.close(fd);
        }
        else if (input == "send")
        {
            cout << "Enter fd:" << endl;
            cin >> fd;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Now send to " << fd << endl;
        }
        else if (input == "quit")
        {
            fd = -1;
            cout << "Now send to all" << endl;
        }
        else if (input == "stop")
        {
            s.stop();
            cout << "stopped" << endl;
        }
        else if (input == "start")
        {
            s.start();
            cout << "started" << endl;
        }
        else if (input == "shutdown")
        {
            s.stop();
            cout << "stopped" << endl;
            exit(0);
        }
        else
        {
            input += "\n";
            if (-1 != fd)
            {
                s.send(fd, input.c_str(), input.size());
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // string addr = argv[1];
    // uint16_t port = stoi(argv[2]);

    string addr = "0.0.0.0";
    uint16_t port = 10000;

    SelectTCPServer s(port, addr);
    s.set_callback_on_recv(recv_cb);
    s.set_callback_on_conn(conn_cb);
    s.set_callback_on_disconn(disconn_cb);
    s.start();

    thread t(send_msg, ref(s));

    while (1)
    {
        // cout << s << endl;
        this_thread::sleep_for(chrono::seconds(10));
    }

    return 0;
}