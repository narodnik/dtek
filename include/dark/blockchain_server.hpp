#ifndef DARK_BLOCKCHAIN_SERVER_HPP
#define DARK_BLOCKCHAIN_SERVER_HPP

#include <bitcoin/bitcoin.hpp>
#include <czmq.h>
#include <dark/blockchain.hpp>

namespace dark {

enum class blockchain_server_command
{
    put = 1,
    get = 2,
    remove = 3,
    exists = 4,
    count = 5
};

struct blockchain_server_request
{
    blockchain_server_command command;
    bc::data_chunk data;
};

class blockchain_server
{
public:
    blockchain_server();
    ~blockchain_server();

    void start();
private:
    blockchain_server_request receive();
    void reply(const blockchain_server_request& request);

    void respond(bc::data_slice data);
    void respond(uint32_t value);

    dark::blockchain chain_;
    zsock_t* socket_ = nullptr;
};

} // namespace dark

#endif

