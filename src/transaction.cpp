#include <dark/transaction.hpp>

#include <dark/utility.hpp>

namespace dark {

schnorr_signature sign(const bc::ec_scalar& secret)
{
    const auto k = new_key();
    const auto R = k * bc::ec_point::G;
    const bc::ec_scalar e = bc::sha256_hash(R.point());
    const auto s = e + k * secret;
    return { R, s };
}
bool verify(const schnorr_signature& signature, const bc::ec_point& key)
{
    const bc::ec_scalar e = bc::sha256_hash(signature.witness.point());
    return signature.response * bc::ec_point::G ==
        signature.witness + e * key;
}

} // namespace dark

