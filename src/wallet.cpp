#include <dark/wallet.hpp>

namespace dark {

#define LITERAL_H \
"02182f2b3da9f6a8538dabac0e4208bad135e93b8f4824c54f2fa1b974ece63762"

const bcs::ec_point ec_point_H = bcs::base16_literal(LITERAL_H);

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
        idx int unique default null, \
        public_point varchar(66) unique, \
        private_key varchar(64), \
        value int \
        )");
}

void wallet::insert(const bcs::ec_point& point,
    const bcs::ec_secret& secret, uint64_t value)
{
    dark::WalletTable wallet_table;
    db_(insert_into(wallet_table).set(
        wallet_table.publicPoint = bcs::encode_base16(point.point()),
        wallet_table.privateKey = bcs::encode_base16(secret),
        wallet_table.value = value));
}
bool wallet::do_update(const bcs::ec_point& point, size_t index)
{
    BITCOIN_ASSERT(exists(point));
    dark::WalletTable wallet_table;
    db_(update(wallet_table).set(
        wallet_table.idx = index).where(
        wallet_table.publicPoint == bcs::encode_base16(point.point())));
    return true;
}

bool wallet::exists(size_t index)
{
    dark::WalletTable wallet_table;
    for(const auto& row: db_(
        select(all_of(wallet_table)).from(wallet_table).unconditionally()))
    {
        size_t row_index = row.idx;
        if (index == row_index)
            return true;
    }
    return false;
}
bool wallet::exists(const bcs::ec_point& point)
{
    const auto point_string = bcs::encode_base16(point.point());
    dark::WalletTable wallet_table;
    for(const auto& row: db_(
        select(all_of(wallet_table)).from(wallet_table).unconditionally()))
    {
        std::string row_point = row.publicPoint;
        if (point_string == row_point)
            return true;
    }
    return false;
}
void wallet::remove(size_t index)
{
    dark::WalletTable wallet_table;
    db_(remove_from(wallet_table).where(wallet_table.idx == index));
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

selected_output_list wallet::select_outputs(
    uint64_t send_value, uint64_t& total_amount)
{
    selected_output_list selected;
    dark::WalletTable wallet_table;

    total_amount = 0;
    for(const auto& row: db_(
        select(all_of(wallet_table)).from(wallet_table).where(
            wallet_table.idx.is_not_null())))
    {
        uint32_t index = row.idx;
        bcs::ec_secret secret;
        bool rc = bcs::decode_base16(secret, row.privateKey);
        BITCOIN_ASSERT(rc);
        selected.push_back(selected_output{
            index,
            secret
        });
        uint64_t value = row.value;
        total_amount += value;
        if (total_amount >= send_value)
            return selected;
    }
    BITCOIN_ASSERT(total_amount < send_value);
    return selected_output_list();
}

} // namespace dark

