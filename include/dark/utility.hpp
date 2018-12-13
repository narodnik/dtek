#ifndef DARK_UTILITY_HPP

#include <bitcoin/bitcoin.hpp>
#include <nlohmann/json.hpp>
#include <dark/transaction.hpp>

namespace dark {

using json = nlohmann::json;

bc::ec_secret new_key();

bc::ec_secret new_key(const bc::data_chunk& seed);

bc::data_chunk new_seed(size_t bit_length=192);

uint32_t random_uint();

dark::transaction transaction_from_json(const json& response);

} // namespace dark

#endif

