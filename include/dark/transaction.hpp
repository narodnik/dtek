#ifndef DARK_TRANSACTION_HPP
#define DARK_TRANSACTION_HPP

#include <bitcoin/bitcoin.hpp>
#include <dark/blockchain.hpp>

namespace dark {

struct schnorr_signature
{
    bc::ec_point witness;
    bc::ec_scalar response;
};

schnorr_signature sign(const bc::ec_scalar& secret,
    const bc::ec_scalar& salt, const bc::ec_point& other_R);
bool verify(const schnorr_signature& signature, const bc::ec_point& key,
    const bc::ec_point& other_R);
bool verify(const schnorr_signature& signature, const bc::ec_point& key);
schnorr_signature aggregate(
    const schnorr_signature& left, const schnorr_signature& right);

typedef output_index_type input_index_type;
typedef std::vector<input_index_type> input_index_list;

struct transaction_rangeproof
{
    bc::point_list commitments;
    bc::ring_signature signature;
};

struct transaction_output
{
    bc::ec_point output;
    transaction_rangeproof rangeproof;
};

typedef std::vector<transaction_output> output_list;

struct transaction_kernel
{
    uint64_t fee;
    bc::ec_point excess;
    schnorr_signature signature;
};

struct transaction
{
    input_index_list inputs;
    output_list outputs;
    transaction_kernel kernel;
};

typedef std::vector<bc::ec_point> outputs_type;

} // namespace dark

#endif

