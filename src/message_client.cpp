#include <dark/message_client.hpp>

#include <nlohmann/json.hpp>

namespace dark {

using json = nlohmann::json;

message_client::message_client()
{
}
message_client::~message_client()
{
    zsock_destroy(&sender_socket_);
    zsock_destroy(&receiver_socket_);
}

void message_client::send(const std::string& message)
{
    if (!sender_socket_)
    {
        sender_socket_ = zsock_new(ZMQ_PUSH);
        zsock_connect(sender_socket_, "tcp://localhost:8888");
    }
    zstr_send(sender_socket_, message.data());
}

std::string message_client::receive()
{
    if (!receiver_socket_)
    {
        receiver_socket_ = zsock_new(ZMQ_SUB);
        zsock_connect(receiver_socket_, "tcp://localhost:8889");
        zsock_set_subscribe(receiver_socket_, "");
        zsys_handler_set(NULL);
    }

    char* message = zstr_recv(receiver_socket_);
    std::string result(message);
    free(message);
    return result;
}

client_worker_thread::client_worker_thread(
    const uint32_t tx_id, const std::string& command)
  : tx_id_(tx_id), command_(command)
{
}

void client_worker_thread::run()
{
    while (true)
    {
        const auto result = client_.receive();
        auto response = json::parse(result);
        if (response.count("tx") && response["tx"].count("id") &&
            response["tx"]["id"].is_number() &&
            response["tx"]["id"].get<uint32_t>() == tx_id_ &&
            response.count("command") &&
            response["command"].is_string() &&
            response["command"].get<std::string>() == command_)
        {
            emit ready(QString::fromStdString(result));
            return;
        }
    }
}

listen_worker_thread::listen_worker_thread(const std::string& username)
  : username_(username)
{
}

void listen_worker_thread::run()
{
    while (true)
    {
        const auto result = client_.receive();
        auto response = json::parse(result);
        if (response.count("command") &&
            response["command"].is_string() &&
            response["command"].get<std::string>() == "send" &&
            response.count("tx") && response["tx"].count("destination") &&
            response["tx"]["destination"].is_string() &&
            response["tx"]["destination"].get<std::string>() == username_)
        {
            emit ready(QString::fromStdString(result));
        }
    }
}

} // namespace dark

