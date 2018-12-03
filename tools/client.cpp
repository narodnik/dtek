#include <iostream>
#include <czmq.h>

class client
{
public:
    client();
    ~client();

    void send(const char* message);

    void start();
private:
    zsock_t* sender_socket_ = nullptr;
    zsock_t* receiver_socket_ = nullptr;
};

client::client()
{
    sender_socket_ = zsock_new(ZMQ_PUSH);
    zsock_connect(sender_socket_, "tcp://localhost:8888");

    receiver_socket_ = zsock_new(ZMQ_SUB);
    zsock_connect(receiver_socket_, "tcp://localhost:8889");
    zsock_set_subscribe(receiver_socket_, "");
}
client::~client()
{
    zsock_destroy(&sender_socket_);
    zsock_destroy(&receiver_socket_);
}

void client::send(const char* message)
{
    zstr_send(sender_socket_, message);
}

void client::start()
{
    zsys_handler_set(NULL);
    while (true)
    {
        char* message = zstr_recv(receiver_socket_);
        std::cout << message << std::endl;
        free(message);
    }
}

int main()
{
    client c;
    c.send("hello");
    c.start();
    return 0;
}

