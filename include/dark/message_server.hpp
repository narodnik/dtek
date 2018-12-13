#ifndef DARK_MESSAGE_SERVER_HPP
#define DARK_MESSAGE_SERVER_HPP

#include <czmq.h>
#include <nlohmann/json.hpp>
#include <dark/blockchain.hpp>

namespace dark {

using json = nlohmann::json;

class message_server
{
public:
    message_server(dark::blockchain& chain);
    ~message_server();
    void start();
    void accept_if_valid(json response);
private:
    zsock_t* receiver_socket_ = nullptr;
    zsock_t* publish_socket_ = nullptr;
    dark::blockchain& chain_;
};

} // namespace dark

#endif

