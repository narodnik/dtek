#ifndef DARK_WALLET_HPP
#define DARK_WALLET_HPP

#include <bitcoin/bitcoin.hpp>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>
#include <sqlcipher/sqlite3.h>
#include "wallet_sql.h"

namespace dark {

namespace sql = sqlpp::sqlite3;

const extern bc::ec_point ec_point_H;

class wallet
{
public:
    wallet(const std::string& filename);

    void insert(size_t index, const bc::ec_point& point,
        const bc::ec_secret& secret, uint64_t value);

    uint64_t balance();
private:
    sql::connection_config config_;
    sql::connection db_;
};

} // namespace dark

#endif

