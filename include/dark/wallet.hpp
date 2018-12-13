#ifndef DARK_WALLET_HPP
#define DARK_WALLET_HPP

#include <bitcoin/bitcoin.hpp>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>
#include <sqlcipher/sqlite3.h>
#include <dark/blockchain.hpp>
#include "wallet_sql.h"

namespace dark {

namespace sql = sqlpp::sqlite3;

const extern bc::ec_point ec_point_H;

struct selected_output
{
    output_index_type index;
    bc::ec_scalar key;
};
typedef std::vector<selected_output> selected_output_list;

class wallet
{
public:
    wallet(const std::string& filename);

    void insert(const bc::ec_point& point,
        const bc::ec_secret& secret, uint64_t value);
    bool do_update(const bc::ec_point& point, size_t index);

    bool exists(size_t index);
    bool exists(const bc::ec_point& point);
    void remove(size_t index);

    uint64_t balance();

    selected_output_list select_outputs(uint64_t send_value);
private:
    sql::connection_config config_;
    sql::connection db_;
};

} // namespace dark

#endif

