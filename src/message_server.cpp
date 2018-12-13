#include <dark/message_server.hpp>

#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

namespace dark {

using json = nlohmann::json;

message_server::message_server()
{
    receiver_socket_ = zsock_new(ZMQ_PULL);
    zsock_bind(receiver_socket_, "tcp://*:8888");

    publish_socket_ = zsock_new(ZMQ_PUB);
    zsock_bind(publish_socket_, "tcp://*:8889");
}
message_server::~message_server()
{
    zsock_destroy(&receiver_socket_);
    zsock_destroy(&publish_socket_);
}
void message_server::start()
{
    zsys_handler_set(NULL);
    while (true)
    {
        char* message = zstr_recv(receiver_socket_);
        std::string result(message);
        free(message);

        const auto response = json::parse(result);
        if (response.count("command") && response["command"] == "broadcast")
        {
            std::cout << "Special case!" << std::endl;
            std::cout << response.dump(4) << std::endl;
        }
        else
            zstr_send(publish_socket_, result.data());
    }
}

} // namespace dark

