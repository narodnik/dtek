#include <dark/message_server.hpp>

#include <iostream>
#include <string>
#include <dark/utility.hpp>
#include <dark/wallet.hpp>

namespace dark {

message_server::message_server(dark::blockchain& chain)
  : chain_(chain)
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
            accept_if_valid(response);
        else
            zstr_send(publish_socket_, result.data());
    }
}

void message_server::accept_if_valid(json response)
{
    const auto tx = transaction_from_json(response);

    // verify outputs and inputs
    bcs::ec_point excess;
    bool is_init = false;
    for (const auto& output: tx.outputs)
    {
        if (!is_init)
        {
            is_init = true;
            excess = output.output;
        }
        else
            excess += output.output;
    }
    for (const auto input: tx.inputs)
    {
        if (input >= chain_.count() || !chain_.exists(input))
        {
            std::cout << "Invalid input. Rejecting tx" << std::endl;
            return;
        }
        auto result = chain_.get(input);
        bcs::ec_compressed point;
        std::copy(result, result + bcs::ec_compressed_size, point.begin());
        excess -= point;
    }
    if (tx.kernel.excess != excess)
    {
        std::cout << "Excess values do not sum. Rejecting tx" << std::endl;
        return;
    }

    // validate attached signature
    if (!dark::verify(tx.kernel.signature, tx.kernel.excess))
    {
        std::cout << "Signature does not verify. Rejecting tx" << std::endl;
        return;
    }

    // verify rangeproofs
    for (const auto& output: tx.outputs)
    {
        const auto& rangeproof = output.rangeproof;

        bcs::key_rings test_rings;
        for (size_t i = 0; i < proofsize; ++i)
        {
            const auto& commitment = rangeproof.commitments[i];
            const uint64_t value_2i = std::pow(2, i);
            test_rings.push_back({
                commitment,
                commitment - bcs::ec_scalar(value_2i) * dark::ec_point_H });
        }

        // Verify rangeproof
        if (!bcs::verify(test_rings, bcs::null_hash, rangeproof.signature))
        {
            std::cout << "Rangeproof failed. Rejecting tx" << std::endl;
            return;
        }
    }

    std::cout << "Accepting transaction..." << std::endl;

    typedef std::vector<output_index_type> index_list;
    index_list removed_indexes, added_indexes;

    for (const auto input: tx.inputs)
    {
        BITCOIN_ASSERT(input < chain_.count());
        BITCOIN_ASSERT(chain_.exists(input));
        chain_.remove(input);
        removed_indexes.push_back(input);
        std::cout << "Removed #" << input << std::endl;
    }
    response["added"] = json::array();
    for (const auto& output: tx.outputs)
    {
        auto index = chain_.put(output.output);
        added_indexes.push_back(index);
        std::cout << "Allocated #" << index << ": "
            << bcs::encode_base16(output.output.point()) << std::endl;
        response["added"].push_back({
            {"index", index},
            {"point", bcs::encode_base16(output.output.point())}
        });
    }

    response["command"] = "final";
    response["removed"] = removed_indexes;
    auto result = response.dump();
    std::cout << "Final stage: " << response.dump(4) << std::endl;
    zstr_send(publish_socket_, result.data());
}

} // namespace dark

