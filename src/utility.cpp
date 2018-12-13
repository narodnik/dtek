#include <dark/utility.hpp>

namespace dark {

bc::ec_secret new_key()
{
    const auto seed = new_seed();
    return new_key(seed);
}

// The key may be invalid, caller may test for null secret.
bc::ec_secret new_key(const bc::data_chunk& seed)
{
    const bc::wallet::hd_private key(seed);
    return key.secret();
}

// Not testable due to lack of random engine injection.
bc::data_chunk new_seed(size_t bit_length)
{
    size_t fill_seed_size = bit_length / bc::byte_bits;
    bc::data_chunk seed(fill_seed_size);
    bc::pseudo_random_fill(seed);
    return seed;
}

uint32_t random_uint()
{
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<uint32_t> uniform(
        0, std::numeric_limits<uint32_t>::max());
    return uniform(generator);
}

transaction transaction_from_json(const json& response)
{
    dark::transaction tx;
    tx.kernel.fee = response["tx"]["kernel"]["fee"].get<uint64_t>();
    bc::ec_compressed excess_point;
    bool rc = bc::decode_base16(
        excess_point, response["tx"]["kernel"]["excess"].get<std::string>());
    BITCOIN_ASSERT(rc);
    tx.kernel.excess = excess_point;
    bc::ec_compressed witness_point;
    rc = bc::decode_base16(
        witness_point,
        response["tx"]["kernel"]["signature"]["witness"].get<std::string>());
    tx.kernel.signature.witness = witness_point;
    BITCOIN_ASSERT(rc);
    bc::ec_secret signature_response;
    rc = bc::decode_base16(
        signature_response,
        response["tx"]["kernel"]["signature"]["response"].get<std::string>());
    BITCOIN_ASSERT(rc);
    tx.kernel.signature.response = signature_response;

    for (auto& input: response["tx"]["inputs"])
    {
        tx.inputs.push_back(input.get<uint32_t>());
    }

    for (auto& output: response["tx"]["outputs"])
    {
        bc::ec_compressed output_point;
        rc = bc::decode_base16(output_point, output["output"]);
        BITCOIN_ASSERT(rc);
        tx.outputs.push_back(dark::transaction_output{
            output_point,
            dark::transaction_rangeproof()
        });
    }
    return tx;
}

} // namespace dark

