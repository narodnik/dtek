#ifndef DARK_UTILITY_HPP

#include <bitcoin/bitcoin.hpp>

namespace dark {

bc::ec_secret new_key();

bc::ec_secret new_key(const bc::data_chunk& seed);

bc::data_chunk new_seed(size_t bit_length=192);

uint32_t random_uint();

} // namespace dark

#endif

