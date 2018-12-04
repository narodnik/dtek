#include <bitcoin/bitcoin.hpp>

#include <boost/range/irange.hpp>
#include <czmq.h>
#include <cxxopts.hpp>
#include <dark/blockchain.hpp>
#include <dark/wallet.hpp>

namespace dark {

typedef size_t input_index_type;
typedef std::vector<input_index_type> input_index_list;

struct transaction_output
{
    bc::ec_point output;
    bc::ring_signature rangeproof;
};

typedef std::vector<transaction_output> output_list;

struct transaction_kernel
{
    uint64_t fee;
    bc::ec_point excess;
};

struct transaction
{
    input_index_list inputs;
    output_list outputs;
    transaction_kernel kernel;
};

typedef std::vector<bc::ec_point> outputs_type;

} // namespace dark

void calc(uint32_t value1, uint32_t value2)
{
    std::cout << "Calculating: " << value1 << " G + "
        << value2 << " H" << std::endl;

    auto secret1 = bc::ec_scalar::zero.secret();
    auto secret2 = bc::ec_scalar::zero.secret();

    auto serial1 = bc::make_unsafe_serializer(secret1.end() - 4);
    serial1.write_4_bytes_big_endian(value1);

    auto serial2 = bc::make_unsafe_serializer(secret2.end() - 4);
    serial2.write_4_bytes_big_endian(value2);

    const auto result = secret1 * bc::ec_point::G + secret2 * bc::ec_point::G;
    std::cout << bc::encode_base16(result.point()) << std::endl;
    std::cout << std::endl;

    dark::blockchain chain;
    auto index = chain.put(result);
    std::cout << "Allocated #" << index << std::endl;
}

void show_balance(dark::wallet& wallet)
{
    std::cout << wallet.balance() << std::endl;
}

bool send_money(dark::wallet& wallet, uint64_t amount)
{
    if (amount > wallet.balance())
    {
        std::cerr << "Error balance too low for transaction" << std::endl;
        return false;
    }
    return true;
}

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

    void run();
private:
    blockchain_server_request receive();
    void reply(const blockchain_server_request& request);

    void respond(bc::data_slice data);
    void respond(uint32_t value);

    dark::blockchain chain_;
    zsock_t* socket_ = nullptr;
};

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

namespace dark {

class blockchain_client
{
public:
    blockchain_client();
    ~blockchain_client();

    output_index_type put(const bc::ec_compressed& point);
    bc::ec_compressed get(const output_index_type index);

    void remove(const output_index_type index);
    bool exists(const output_index_type index);

    output_index_type count();

private:
    void send_request(blockchain_server_command command, bc::data_slice data);
    void send_request(blockchain_server_command command, uint32_t value);

    bc::data_chunk receive_response();

    zsock_t* socket_ = nullptr;
};

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
bc::ec_compressed blockchain_client::get(const output_index_type index)
{
    send_request(blockchain_server_command::get, index);

    auto response_data = receive_response();
    bc::ec_compressed result;
    std::copy(response_data.begin(), response_data.end(), result.begin());
    return result;
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

void show_help()
{
    std::cout << "Usage: darktech [OPTION]" << std::endl;
    std::cout << std::endl;
    std::cout << "  -h, --help\tshow this help text" << std::endl;
    std::cout << "  -w, --write POINT\twrite ec compressed point" << std::endl;
    std::cout << "  -r, --read\tread all the blocks" << std::endl;
    std::cout << "  -d, --delete INDEX\tdelete block" << std::endl;
    std::cout << "  --c1 NUM\tcalculate point #1" << std::endl;
    std::cout << "  --c2 NUM\tcalculate point #2" << std::endl;
}

bool write_point(const std::string& point_string)
{
    bc::ec_compressed point;
    if (!bc::decode_base16(point, point_string))
    {
        std::cerr << "Error loading point." << std::endl;
        return false;
    }

    //dark::blockchain chain;
    dark::blockchain_client chain;
    auto index = chain.put(point);
    std::cout << "Allocated #" << index << std::endl;
    return true;
}

void read_all()
{
    //dark::blockchain chain;
    dark::blockchain_client chain;
    for (auto i: boost::irange(chain.count()))
    {
        if (!chain.exists(i))
            continue;
        auto point = chain.get(i);
        std::cout << "#" << i << " " << bc::encode_base16(point) << std::endl;
    }
}

bool remove_point(size_t index)
{
    //dark::blockchain chain;
    dark::blockchain_client chain;
    if (index >= chain.count())
    {
        std::cerr << "Error removing invalid index." << std::endl;
        return false;
    }
    if (!chain.exists(index))
    {
        std::cerr << "Error already deleted. Doing nothing." << std::endl;
        return false;
    }
    chain.remove(index);
    return true;
}

void add_output(dark::wallet& wallet, uint64_t value)
{
    // Create private key
    bc::ec_secret secret;
    bc::pseudo_random::fill(secret);

    auto value_secret = bc::ec_scalar::zero.secret();
    auto serial = bc::make_unsafe_serializer(value_secret.end() - 4);
    serial.write_4_bytes_big_endian(value);

    std::cout << "secret: " << bc::encode_base16(secret) << std::endl;
    std::cout << "scalar: " << bc::encode_base16(value_secret)
        << std::endl;

    auto point = secret * bc::ec_point::G + value_secret * dark::ec_point_H;

    std::cout << "point: " << bc::encode_base16(point.point()) << std::endl;

    // Allocate index
    // Add to blockchain
    //dark::blockchain chain;
    dark::blockchain_client chain;
    auto index = chain.put(point);

    // Add to wallet
    wallet.insert(index, point, secret, value);
}

int main(int argc, char** argv)
{
    cxxopts::Options options("darktech", "dark polytechnology");
    options.add_options()
        ("h,help", "Show program help")
        ("write", "Write point",  cxxopts::value<std::string>())
        ("w,wallet", "Path to wallet",  cxxopts::value<std::string>())
        ("r,read", "Read all points")
        ("d,delete", "Delete row", cxxopts::value<size_t>())
        ("c1", "Calculate point #1", cxxopts::value<uint32_t>())
        ("c2", "Calculate point #2", cxxopts::value<uint32_t>())
        ("b,balance", "Show balance")
        ("s,send", "Send money", cxxopts::value<uint64_t>())
        ("a,add", "Add fake output", cxxopts::value<uint64_t>())
        ("server", "Run blockchain server")
    ;
    auto result = options.parse(argc, argv);

    std::string wallet_path = "wallet.db";
    if (result.count("wallet"))
        wallet_path = result["wallet"].as<std::string>();

    if (result.count("help"))
    {
        show_help();
        return 1;
    }
    else if (result.count("write"))
    {
        return write_point(result["write"].as<std::string>()) ? 0 : -1;
    }
    else if (result.count("read"))
    {
        read_all();
    }
    else if (result.count("delete"))
    {
        return remove_point(result["delete"].as<size_t>()) ? 0 : -1;
    }
    else if (result.count("c1"))
    {
        if (!result.count("c2"))
        {
            std::cerr << "Error second argument required" << std::endl;
            return -1;
        }
        uint32_t value1 = result["c1"].as<uint32_t>();
        uint32_t value2 = result["c2"].as<uint32_t>();
        calc(value1, value2);
    }
    else if (result.count("balance"))
    {
        dark::wallet wallet(wallet_path);
        show_balance(wallet);
    }
    else if (result.count("send"))
    {
        dark::wallet wallet(wallet_path);
        uint64_t amount = result["send"].as<uint64_t>();
        return send_money(wallet, amount) ? 0 : -1;
    }
    else if (result.count("add"))
    {
        dark::wallet wallet(wallet_path);
        uint64_t value = result["add"].as<uint64_t>();
        add_output(wallet, value);
    }
    else if (result.count("server"))
    {
        blockchain_server server;
        server.run();
    }

    return 0;
}

