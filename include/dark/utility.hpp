#ifndef DARK_UTILITY_HPP

#include <bitcoin/system.hpp>
#include <nlohmann/json.hpp>
#include <dark/transaction.hpp>

namespace dark {

namespace bcs = bc::system;
using json = nlohmann::json;

bcs::ec_secret new_key();

bcs::ec_secret new_key(const bcs::data_chunk& seed);

bcs::data_chunk new_seed(size_t bit_length=192);

uint32_t random_uint();

dark::transaction transaction_from_json(const json& response);

constexpr size_t proofsize = 64;

json rangeproof_to_json(const transaction_rangeproof& rangeproof);
transaction_rangeproof rangeproof_from_json(const json& response);

} // namespace dark

#endif

