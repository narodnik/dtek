#include <dark/transaction.hpp>

#include <iostream>
#include <dark/utility.hpp>

namespace dark {

schnorr_signature sign(const bc::ec_scalar& secret,
    const bc::ec_scalar& k, const bc::ec_point& other_R)
{
    const auto R = k * bc::ec_point::G + other_R;
    const auto combined_R = R + other_R;
    const bc::ec_scalar e = bc::sha256_hash(combined_R.point());
    const auto s = e + k * secret;
    return { R, s };
}
bool verify(const schnorr_signature& signature, const bc::ec_point& key,
    const bc::ec_point& combined_R)
{
    const bc::ec_scalar e = bc::sha256_hash(combined_R.point());
    const auto sG = signature.response * bc::ec_point::G;
    const auto R = signature.witness;
    const auto eP = e * key;
    return sG == R + eP;
}
bool verify(const schnorr_signature& signature, const bc::ec_point& key)
{
    return verify(signature, key, signature.witness);
}
schnorr_signature aggregate(
    const schnorr_signature& left, const schnorr_signature& right)
{
    return {
        left.witness + right.witness,
        left.response + right.response
    };
}

} // namespace dark

