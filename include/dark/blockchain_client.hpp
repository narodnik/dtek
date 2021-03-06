#ifndef DARK_BLOCKCHAIN_CLIENT_HPP
#define DARK_BLOCKCHAIN_CLIENT_HPP

#include <dark/blockchain_server.hpp>

namespace dark {

struct get_result
{
    bcs::ec_compressed point;
    std::time_t time;
};

class blockchain_client
{
public:
    blockchain_client();
    ~blockchain_client();

    output_index_type put(const bcs::ec_compressed& point);
    get_result get(const output_index_type index);

    void remove(const output_index_type index);
    bool exists(const output_index_type index);

    output_index_type count();

private:
    void send_request(blockchain_server_command command, bcs::data_slice data);
    void send_request(blockchain_server_command command, uint32_t value);

    bcs::data_chunk receive_response();

    zsock_t* socket_ = nullptr;
};

} // namespace dark

#endif

