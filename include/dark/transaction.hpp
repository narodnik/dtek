#ifndef DARK_TRANSACTION_HPP
#define DARK_TRANSACTION_HPP

#include <bitcoin/system.hpp>
#include <dark/blockchain.hpp>

namespace dark {

namespace bcs = bc::system;

struct schnorr_signature
{
    bcs::ec_point witness;
    bcs::ec_scalar response;
};

schnorr_signature sign(const bcs::ec_scalar& secret,
    const bcs::ec_scalar& salt, const bcs::ec_point& other_R);
bool verify(const schnorr_signature& signature, const bcs::ec_point& key,
    const bcs::ec_point& other_R);
bool verify(const schnorr_signature& signature, const bcs::ec_point& key);
schnorr_signature aggregate(
    const schnorr_signature& left, const schnorr_signature& right);

typedef output_index_type input_index_type;
typedef std::vector<input_index_type> input_index_list;

struct transaction_rangeproof
{
    bcs::point_list commitments;
    bcs::ring_signature signature;
};

struct transaction_output
{
    bcs::ec_point output;
    transaction_rangeproof rangeproof;
};

typedef std::vector<transaction_output> output_list;

struct transaction_kernel
{
    uint64_t fee;
    bcs::ec_point excess;
    schnorr_signature signature;
};

struct transaction
{
    input_index_list inputs;
    output_list outputs;
    transaction_kernel kernel;
};

typedef std::vector<bcs::ec_point> outputs_type;

} // namespace dark

#endif

