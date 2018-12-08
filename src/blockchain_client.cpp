#include <dark/blockchain_client.hpp>

namespace dark {

blockchain_client::blockchain_client()
{
    socket_ = zsock_new(ZMQ_REQ);
    zsock_connect(socket_, "tcp://localhost:8887");
}
blockchain_client::~blockchain_client()
{
    zsock_destroy(&socket_);
}

output_index_type blockchain_client::put(const bc::ec_compressed& point)
{
    send_request(blockchain_server_command::put, point);

    auto response_data = receive_response();
    auto deserial = bc::make_unsafe_deserializer(response_data.begin());
    return deserial.read_4_bytes_little_endian();
}
get_result blockchain_client::get(const output_index_type index)
{
    send_request(blockchain_server_command::get, index);

    auto response_data = receive_response();
    bc::ec_compressed result;
    std::copy(response_data.begin(), response_data.end(), result.begin());

    auto deserial = bc::make_unsafe_deserializer(
        response_data.begin() + bc::ec_compressed_size);
    return { result, deserial.read_4_bytes_little_endian() };
}

void blockchain_client::remove(const output_index_type index)
{
    send_request(blockchain_server_command::remove, index);

    auto response_data = receive_response();
    BITCOIN_ASSERT(response_data.empty());
}
bool blockchain_client::exists(const output_index_type index)
{
    send_request(blockchain_server_command::exists, index);

    auto response_data = receive_response();
    auto deserial = bc::make_unsafe_deserializer(response_data.begin());
    return deserial.read_4_bytes_little_endian();
}

output_index_type blockchain_client::count()
{
    send_request(blockchain_server_command::count, bc::data_chunk());

    auto response_data = receive_response();
    auto deserial = bc::make_unsafe_deserializer(response_data.begin());
    return deserial.read_4_bytes_little_endian();
}

void blockchain_client::send_request(blockchain_server_command command,
    bc::data_slice data)
{
    zmsg_t* message = zmsg_new();
    assert(message);

    bc::data_chunk command_data(1);
    auto serial = bc::make_unsafe_serializer(command_data.begin());
    serial.write_byte(static_cast<uint8_t>(command));

    zframe_t *frame = zframe_new(command_data.data(), command_data.size());
    assert(frame);
    zmsg_append(message, &frame);

    frame = zframe_new(data.data(), data.size());
    assert(frame);
    zmsg_append(message, &frame);

    assert(zmsg_size(message) == 2);
    int rc = zmsg_send(&message, socket_);
    assert(!message);
    assert(rc == 0);
}
void blockchain_client::send_request(
    blockchain_server_command command, uint32_t value)
{
    bc::data_chunk data(4);
    auto serial = bc::make_unsafe_serializer(data.begin());
    serial.write_4_bytes_little_endian(value);
    send_request(command, data);
}

bc::data_chunk blockchain_client::receive_response()
{
    zmsg_t* message = zmsg_recv(socket_);
    assert(message);
    assert(zmsg_size(message) == 1);

    zframe_t* frame = zmsg_pop(message);
    assert(frame);
    auto* data = zframe_data(frame);
    auto size = zframe_size(frame);
    bc::data_chunk result(size);
    std::copy(data, data + size, result.begin());
    zframe_destroy(&frame);

    zmsg_destroy(&message);

    return result;
}

} // namespace dark

