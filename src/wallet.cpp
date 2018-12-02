#include <dark/wallet.hpp>

namespace dark {

#define LITERAL_H \
"02182f2b3da9f6a8538dabac0e4208bad135e93b8f4824c54f2fa1b974ece63762"

const bc::ec_point ec_point_H = bc::base16_literal(LITERAL_H);

sql::connection_config generate_config(const std::string& filename)
{
    sql::connection_config config;
    config.path_to_database = filename;
    config.flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    config.debug = true;
    config.password = "hello";
    return config;
}

wallet::wallet(const std::string& filename)
  : config_(generate_config(filename)), db_(config_)
{
    db_.execute("create table if not exists wallet_table ( \
        idx int unique, \
        public_point varchar(66) unique, \
        private_key varchar(64), \
        value int \
        )");
}

void wallet::insert(size_t index, const bc::ec_point& point,
    const bc::ec_secret& secret, uint64_t value)
{
    dark::WalletTable wallet_table;
    db_(insert_into(wallet_table).set(
        wallet_table.idx = index,
        wallet_table.publicPoint = bc::encode_base16(point.point()),
        wallet_table.privateKey = bc::encode_base16(secret),
        wallet_table.value = value));
}

uint64_t wallet::balance()
{
    dark::WalletTable wallet_table;
    //db(insert_into(tab).default_values());

    uint64_t balance = 0;
    for(const auto& row: db_(
        select(all_of(wallet_table)).from(wallet_table).unconditionally()))
    {
        uint64_t value = row.value;
        balance += value;
    }
    return balance;
}

} // namespace dark

