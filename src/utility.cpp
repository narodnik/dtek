#include <dark/utility.hpp>

namespace dark {

bcs::ec_secret new_key()
{
    const auto seed = new_seed();
    return new_key(seed);
}

// The key may be invalid, caller may test for null secret.
bcs::ec_secret new_key(const bcs::data_chunk& seed)
{
    const bcs::wallet::hd_private key(seed);
    return key.secret();
}

// Not testable due to lack of random engine injection.
bcs::data_chunk new_seed(size_t bit_length)
{
    size_t fill_seed_size = bit_length / bc::byte_bits;
    bcs::data_chunk seed(fill_seed_size);
    bcs::pseudo_random_fill(seed);
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
    bcs::ec_compressed excess_point;
    bool rc = bcs::decode_base16(
        excess_point, response["tx"]["kernel"]["excess"].get<std::string>());
    BITCOIN_ASSERT(rc);
    tx.kernel.excess = excess_point;
    bcs::ec_compressed witness_point;
    rc = bcs::decode_base16(
        witness_point,
        response["tx"]["kernel"]["signature"]["witness"].get<std::string>());
    tx.kernel.signature.witness = witness_point;
    BITCOIN_ASSERT(rc);
    bcs::ec_secret signature_response;
    rc = bcs::decode_base16(
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
        bcs::ec_compressed output_point;
        rc = bcs::decode_base16(output_point, output["output"]);
        BITCOIN_ASSERT(rc);
        tx.outputs.push_back(dark::transaction_output{
            output_point,
            rangeproof_from_json(output["rangeproof"])
        });
    }
    return tx;
}

json rangeproof_to_json(const transaction_rangeproof& rangeproof)
{
    json result = {
        {"commitments", json::array()},
        {"signature", {
            {"challenge", "XX"},
            {"proofs", json::array()}
    }}};
    for (const auto& commitment: rangeproof.commitments)
    {
        result["commitments"].push_back(
            bcs::encode_base16(commitment));
    }
    result["signature"]["challenge"] = bcs::encode_base16(
        rangeproof.signature.challenge);
    for (const auto& secret_list: rangeproof.signature.proofs)
    {
        auto secrets = json::array();
        for (const auto& secret: secret_list)
            secrets.push_back(bcs::encode_base16(secret));
        result["signature"]["proofs"].push_back(secrets);
    }
    return result;
}

transaction_rangeproof rangeproof_from_json(const json& response)
{
    transaction_rangeproof rangeproof;
    for (auto commitment_string: response["commitments"])
    {
        bcs::ec_compressed commitment;
        bool rc = bcs::decode_base16(commitment, commitment_string);
        BITCOIN_ASSERT(rc);
        rangeproof.commitments.push_back(commitment);
    }
    bcs::ec_secret challenge;
    bool rc = bcs::decode_base16(challenge, response["signature"]["challenge"]);
    BITCOIN_ASSERT(rc);
    rangeproof.signature.challenge = challenge;
    for (auto proofs_list: response["signature"]["proofs"])
    {
        bcs::secret_list secrets;
        for (auto proof_string: proofs_list)
        {
            bcs::ec_secret proof;
            rc = bcs::decode_base16(proof, proof_string);
            BITCOIN_ASSERT(rc);
            secrets.push_back(proof);
        }
        rangeproof.signature.proofs.push_back(secrets);
    }
    return rangeproof;
}

} // namespace dark

