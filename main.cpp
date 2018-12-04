#include <bitcoin/bitcoin.hpp>

#include <boost/range/irange.hpp>
#include <cxxopts.hpp>
#include <dark/blockchain_client.hpp>
#include <dark/blockchain_server.hpp>
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
        dark::blockchain_server server;
        server.run();
    }

    return 0;
}

