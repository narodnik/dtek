#ifndef DARK_MESSAGE_SERVER_HPP
#define DARK_MESSAGE_SERVER_HPP

#include <czmq.h>

namespace dark {

class message_server
{
public:
    message_server();
    ~message_server();
    void start();
private:
    zsock_t* receiver_socket_ = nullptr;
    zsock_t* publish_socket_ = nullptr;
};

} // namespace dark

#endif

