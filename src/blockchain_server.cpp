#include <dark/blockchain_server.hpp>

namespace dark {

blockchain_server::blockchain_server()
{
    socket_ = zsock_new(ZMQ_REP);
    zsock_bind(socket_, "tcp://*:8887");
}
blockchain_server::~blockchain_server()
{
    zsock_destroy(&socket_);
}

void blockchain_server::run()
{
    zsys_handler_set(NULL);
    while (true)
    {
        auto request = receive();

        reply(request);
    }
}

blockchain_server_request blockchain_server::receive()
{
    blockchain_server_request request;

    zmsg_t* message = zmsg_recv(socket_);
    assert(message);
    assert(zmsg_size(message) == 2);

    zframe_t* frame = zmsg_pop(message);
    assert(frame);
    auto deserial = bc::make_unsafe_deserializer(zframe_data(frame));
    request.command = static_cast<blockchain_server_command>(
        deserial.read_byte());
    zframe_destroy(&frame);

    frame = zmsg_pop(message);
    assert(frame);
    auto* data = zframe_data(frame);
    size_t data_size = zframe_size(frame);
    request.data.resize(data_size);
    std::copy(data, data + data_size, request.data.begin());
    zframe_destroy(&frame);

    zmsg_destroy(&message);

    return request;
}

void blockchain_server::reply(const blockchain_server_request& request)
{
    switch (request.command)
    {
        case blockchain_server_command::put:
        {
            // Deserialize request arguments
            BITCOIN_ASSERT(request.data.size() == bc::ec_compressed_size);
            bc::ec_compressed point;
            std::copy(request.data.begin(), request.data.end(), point.begin());
            // Blockchain call
            auto index = chain_.put(point);
            std::cout << "put(" << bc::encode_base16(point) << ") -> "
                << index << std::endl;
            // Send response
            respond(index);
            break;
        }
        case blockchain_server_command::get:
        {
            // Deserialize request arguments
            BITCOIN_ASSERT(request.data.size() == 4);
            auto deserial = bc::make_unsafe_deserializer(request.data.begin());
            auto index = deserial.read_4_bytes_little_endian();
            // Blockchain call
            auto point = chain_.get(index);
            std::cout << "get(" << index << ") -> "
                << bc::encode_base16(point) << std::endl;
            // Send response
            respond(point);
            break;
        }
        case blockchain_server_command::remove:
        {
            // Deserialize request arguments
            BITCOIN_ASSERT(request.data.size() == 4);
            auto deserial = bc::make_unsafe_deserializer(request.data.begin());
            auto index = deserial.read_4_bytes_little_endian();
            // Blockchain call
            chain_.remove(index);
            std::cout << "remove(" << index << ")" << std::endl;
            // Send response
            respond(bc::data_chunk());
            break;
        }
        case blockchain_server_command::exists:
        {
            // Deserialize request arguments
            BITCOIN_ASSERT(request.data.size() == 4);
            auto deserial = bc::make_unsafe_deserializer(request.data.begin());
            auto index = deserial.read_4_bytes_little_endian();
            // Blockchain call
            bool exists = chain_.exists(index);
            std::cout << "exists(" << index << ") -> " << exists << std::endl;
            // Send response
            respond(exists ? 1 : 0);
            break;
        }
        case blockchain_server_command::count:
        {
            // No request arguments for this call
            BITCOIN_ASSERT(request.data.empty());
            // Blockchain call
            auto count = chain_.count();
            std::cout << "count() -> " << count << std::endl;
            // Send response
            respond(count);
            break;
        }
        default:
            std::cerr << "Error dropping command" << std::endl;
    }
}

void blockchain_server::respond(bc::data_slice data)
{
    zmsg_t* message = zmsg_new();
    assert(message);
    zframe_t *frame = zframe_new(data.data(), data.size());
    assert(frame);
    zmsg_append(message, &frame);
    assert(zmsg_size(message) == 1);
    int rc = zmsg_send(&message, socket_);
    assert(message == NULL);
    assert(rc == 0);
}
void blockchain_server::respond(uint32_t value)
{
    bc::data_chunk data(4);
    auto serial = bc::make_unsafe_serializer(data.begin());
    serial.write_4_bytes_little_endian(value);
    respond(data);
}

} // namespace dark

