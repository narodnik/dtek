#include <bitcoin/system.hpp>

#include <thread>
#include <boost/optional.hpp>
#include <cxxopts.hpp>
#include <QDateTime>
#include <QMessageBox>
#include <QThread>
#include <nlohmann/json.hpp>
#include <dark/blockchain_client.hpp>
#include <dark/blockchain_server.hpp>
#include <dark/message_client.hpp>
#include <dark/message_server.hpp>
#include <dark/transaction.hpp>
#include <dark/utility.hpp>
#include <dark/wallet.hpp>
#include "ui_darkwallet.h"

namespace bcs = bc::system;
using json = nlohmann::json;

typedef std::unordered_map<uint32_t, bcs::ec_scalar> keys_map_type;

typedef std::function<void ()> update_balance_callback;

keys_map_type keys_map;

bool is_bit_set(uint64_t value, size_t i)
{
    const uint64_t value_2i = std::pow(2, i);
    return (value & value_2i) > 0;
}

void calc(uint32_t value_1, uint32_t value_2)
{
    std::cout << "Calculating: " << value_1 << " G + "
        << value_2 << " H" << std::endl;

    const auto result =
        bcs::ec_scalar(value_1) * bcs::ec_point::G +
        bcs::ec_scalar(value_2) * dark::ec_point_H;
    std::cout << bcs::encode_base16(result.point()) << std::endl;
    std::cout << std::endl;

    dark::blockchain chain;
    auto index = chain.put(result);
    std::cout << "Allocated #" << index << std::endl;
}

void show_balance(dark::wallet& wallet)
{
    std::cout << wallet.balance() << std::endl;
}

void do_update_balance(dark::wallet& wallet, QLabel* balance_label)
{
    auto balance_string = QString::fromStdString(
        bcs::encode_base10(wallet.balance(), 8));
    balance_label->setText(balance_string);
}

struct assign_output_result
{
    bcs::ec_scalar secret;
    bcs::ec_point point;
    dark::transaction_rangeproof rangeproof;
};

assign_output_result assign_output(uint64_t value, std::ostream& stream)
{
    stream << "assign_output(" << value << ")" << std::endl;

    typedef std::array<bcs::ec_scalar, dark::proofsize> subkeys_list;

    subkeys_list subkeys;
    auto secret = bcs::ec_scalar::zero;

    // Create private key by summing 64 sub private keys
    // The 64 subkeys will be used for constructing the rangeproof
    for (auto& subkey: subkeys)
    {
        subkey = dark::new_key();
        secret += subkey;
    }
    BITCOIN_ASSERT(secret);
    stream << "secret: " << bcs::encode_base16(secret.secret()) << std::endl;

    auto point =
        secret * bcs::ec_point::G + bcs::ec_scalar(value) * dark::ec_point_H;

    stream << "point: " << bcs::encode_base16(point.point()) << std::endl;

    // [d_1 G + v_(0|1) H] + [d_2 G + v_(0|2) H] + [d_3 G + v_(0|4) H] + ...
    dark::transaction_rangeproof rangeproof;
    rangeproof.commitments.reserve(dark::proofsize);
    // Used for making the signature
    bcs::key_rings rangeproof_rings;
    bcs::secret_list rangeproof_secrets, rangeproof_salts;
    uint64_t value_checker = 0;
    for (size_t i = 0; i < dark::proofsize; ++i)
    {
        BITCOIN_ASSERT(i < subkeys.size());
        const auto& subkey = subkeys[i];

        // v = 0
        const auto public_key = subkey * bcs::ec_point::G;
        // v = 2^i
        uint64_t value_2i = std::pow(2, i);
        const auto value_point = bcs::ec_scalar(value_2i) * dark::ec_point_H;

        if (is_bit_set(value, i))
        {
            value_checker += value_2i;
            // d G + v H
            rangeproof.commitments.push_back(public_key + value_point);

            // Second key is the valid one we sign with
            // commitment - v H
            rangeproof_rings.push_back({
                public_key + value_point, public_key });
        }
        else
        {
            // d G
            rangeproof.commitments.push_back(public_key);

            // First key is the valid one we sign with
            // commitment
            rangeproof_rings.push_back({
                public_key, public_key - value_point });
        }

        // 2 keys
        rangeproof.signature.proofs.push_back({
            dark::new_key(), dark::new_key() });

        rangeproof_secrets.push_back(subkey);
        rangeproof_salts.push_back(dark::new_key());
    }
    stream << "Assigned: " << value_checker << std::endl;

    // Safety check
    bcs::ec_compressed result;
    bcs::ec_sum(result, rangeproof.commitments);
    BITCOIN_ASSERT(point.point() == result);

    // Check each public key has a correct secret key
    const auto rings_size = rangeproof_rings.size();
    BITCOIN_ASSERT(rangeproof_secrets.size() == rings_size);
    BITCOIN_ASSERT(rangeproof_salts.size() == rings_size);
    BITCOIN_ASSERT(rangeproof.signature.proofs.size() == rings_size);
    for (size_t i = 0; i < rings_size; ++i)
    {
        const auto& secret = rangeproof_secrets[i];
        const auto& ring = rangeproof_rings[i];
        BITCOIN_ASSERT(ring.size() == rangeproof.signature.proofs[i].size());
        BITCOIN_ASSERT(ring.size() == 2);

        const auto key = secret * bcs::ec_point::G;
        BITCOIN_ASSERT(key.point() == ring[0] || key.point() == ring[1]);
    }

    bool rc = bcs::sign(rangeproof.signature, rangeproof_secrets,
        rangeproof_rings, bcs::null_hash, rangeproof_salts);
    BITCOIN_ASSERT(rc);

    BITCOIN_ASSERT(rangeproof.commitments.size() == dark::proofsize);
    bcs::key_rings test_rings;
    for (size_t i = 0; i < dark::proofsize; ++i)
    {
        const auto& commitment = rangeproof.commitments[i];
        const uint64_t value_2i = std::pow(2, i);
        test_rings.push_back({
            commitment,
            commitment - bcs::ec_scalar(value_2i) * dark::ec_point_H });
    }

    // Verify rangeproof
    rc = bcs::verify(test_rings, bcs::null_hash, rangeproof.signature);
    BITCOIN_ASSERT(rc);

    stream << "Rangeproof checks out." << std::endl;

    return { secret, point, rangeproof };
}

void add_output(dark::wallet& wallet, uint64_t value)
{
    // Create private key
    bcs::ec_secret secret;
    bcs::pseudo_random::fill(secret);

    auto value_scalar = bcs::ec_scalar(value);

    std::cout << "secret: " << bcs::encode_base16(secret) << std::endl;
    std::cout << "scalar: " << bcs::encode_base16(value_scalar.secret())
        << std::endl;

    auto point = secret * bcs::ec_point::G + value_scalar * dark::ec_point_H;

    std::cout << "point: " << bcs::encode_base16(point.point()) << std::endl;

    // Allocate index
    // Add to blockchain
    //dark::blockchain chain;
    dark::blockchain_client chain;
    auto index = chain.put(point);

    // Add to wallet
    wallet.insert(point, secret, value);
    wallet.do_update(point, index);
}

void final_update_wallet(dark::wallet& wallet,
    const std::string& response_string,
    std::ostream& stream,
    update_balance_callback update_balance)
{
    const auto response = json::parse(response_string);
    stream << "Final response!" << response.dump(4) << std::endl;

    for (auto index_obj: response["removed"])
    {
        auto index = index_obj.get<uint32_t>();
        if (wallet.exists(index))
        {
            stream << "Removing index #" << index << std::endl;
            wallet.remove(index);
        }
    }

    // Add to wallet
    for (auto added: response["added"])
    {
        auto index = added["index"].get<uint32_t>();
        auto point_string = added["point"].get<std::string>();
        bcs::ec_compressed point;
        bool rc = bcs::decode_base16(point, point_string);
        BITCOIN_ASSERT(rc);
        if (wallet.exists(point))
        {
            stream << "Added index #" << index << " for "
                << point_string << std::endl;
            wallet.do_update(point, index);
        }
    }

    update_balance();
}


template <typename ShowErrorFunction>
void send_money_2(const std::string& username,
    dark::wallet& wallet, dark::message_client& client,
    const std::string& destination, uint64_t amount,
    const std::string& response, const uint32_t tx_id,
    std::ostream& stream, ShowErrorFunction show_error,
    update_balance_callback update_balance);

template <typename ShowErrorFunction>
bool send_money(const std::string& username,
    dark::wallet& wallet, dark::message_client& client,
    const std::string& destination, uint64_t amount,
    std::ostream& stream, ShowErrorFunction show_error,
    update_balance_callback update_balance)
{
    uint64_t balance = wallet.balance();

    if (amount > balance)
    {
        show_error("Balance too low for transaction.");
        return false;
    }

    if (amount == 0)
    {
        show_error("Cannot send 0.");
        return false;
    }

    stream << username << " sending: " << amount
        << " to " << destination << std::endl;

    const auto tx_id = dark::random_uint();

    auto requested_keys = 
        [=, &wallet, &client, &stream](const QString& response)
    {
        send_money_2(username, wallet, client, destination, amount,
            response.toStdString(), tx_id, stream, show_error, update_balance);
    };

    dark::client_worker_thread *preworker = new dark::client_worker_thread(
        tx_id, "request_send_reply");
    QObject::connect(preworker, &dark::client_worker_thread::ready,
        QCoreApplication::instance(), requested_keys);
    QObject::connect(preworker, &dark::client_worker_thread::finished,
        preworker, &QObject::deleteLater);
    preworker->start();

    json send_json = {
        {"command", "request_send"},
        {"tx", {
            {"id", tx_id},
            {"destination", destination}
        }}
    };
    client.send(send_json.dump());

    return true;
}

template <typename ShowErrorFunction>
void send_money_2(const std::string& username,
    dark::wallet& wallet, dark::message_client& client,
    const std::string& destination, uint64_t amount,
    const std::string& response_other, const uint32_t tx_id,
    std::ostream& stream, ShowErrorFunction show_error,
    update_balance_callback update_balance)
{
    auto response_keys = json::parse(response_other);
    bcs::ec_compressed other_witness_point;
    bool rc = bcs::decode_base16(
        other_witness_point, response_keys["witness_i"].get<std::string>());
    BITCOIN_ASSERT(rc);
    stream << "Received witness_i of: "
        << bcs::encode_base16(other_witness_point) << std::endl;
    const bcs::ec_point witness_2 = other_witness_point;

    const uint64_t balance = wallet.balance();
    BITCOIN_ASSERT(amount <= balance);

    uint64_t total_amount = 0;
    auto selected = wallet.select_outputs(amount, total_amount);
    stream << "Selected:";
    for (const auto& row: selected)
        stream << " " << row.index;
    stream << std::endl;

    dark::transaction tx;
    tx.kernel.fee = 0;
    for (const auto& row: selected)
        tx.inputs.push_back(row.index);

    typedef boost::optional<assign_output_result> optional_output;
    optional_output change_output;

    // If we have remaining coins after sending then compute change output
    if (amount < total_amount)
    {
        // calculate change amount
        auto change_amount = total_amount - amount;
        BITCOIN_ASSERT(amount + change_amount == total_amount);
        stream << "Change: " << change_amount << std::endl;
        // Create change output
        // Create 64 private keys, which are used for the rangeproof
        change_output = assign_output(change_amount, stream);
        // Modify transaction
        tx.outputs.push_back(dark::transaction_output{
            change_output->point,
            change_output->rangeproof
        });
        wallet.insert(
            change_output->point, change_output->secret, change_amount);
    }

    // compute excess
    auto excess_secret = bcs::ec_scalar::zero;
    for (const auto& row: selected)
        excess_secret -= row.key;

    if (change_output)
        excess_secret += change_output->secret;

    tx.kernel.excess = excess_secret * bcs::ec_point::G;
    stream << "Excess: "
        << bcs::encode_base16(tx.kernel.excess.point()) << std::endl;

    // compute signature
    const auto k = dark::new_key();
    const auto witness_1 = k * bcs::ec_point::G;
    const auto combined_witness = witness_1 + witness_2;
    tx.kernel.signature = dark::sign(excess_secret, k, combined_witness);

    // verify signature for correctness
    stream << "  (R, s) = ("
        << bcs::encode_base16(tx.kernel.signature.witness.point()) << ", "
        << bcs::encode_base16(tx.kernel.signature.response.secret())
        << ")" << std::endl;
    stream << "  P = "
        << bcs::encode_base16(tx.kernel.excess.point()) << std::endl;
    stream << "  R_1 = "
        << bcs::encode_base16(witness_1.point()) << std::endl;
    stream << "  R_2 = "
        << bcs::encode_base16(witness_2.point()) << std::endl;
    stream << "  R = "
        << bcs::encode_base16(combined_witness.point()) << std::endl;
    BITCOIN_ASSERT(dark::verify(tx.kernel.signature, tx.kernel.excess,
        combined_witness));
    stream << "Signature computed and verified" << std::endl;

    // Create json to send
    // send tx, amount with command
    json send_json = {
        {"command", "send"},
        {"tx", {
            {"id", tx_id},
            {"destination", destination},
            {"inputs", json::array()},
            {"outputs", json::array()},
            {"kernel", {
                {"fee", tx.kernel.fee},
                {"excess", bcs::encode_base16(tx.kernel.excess.point())},
                {"signature", {
                    {"witness", bcs::encode_base16(
                        tx.kernel.signature.witness.point())},
                    {"response", bcs::encode_base16(
                        tx.kernel.signature.response.secret())}
                }}
        }}}},
        {"amount", amount},
        {"witness_1", bcs::encode_base16(witness_1.point())}
    };
    for (const auto input: tx.inputs)
    {
        send_json["tx"]["inputs"].push_back(input);
    }
    for (const auto& output: tx.outputs)
    {
        send_json["tx"]["outputs"].push_back({
            {"output", bcs::encode_base16(output.output.point())},
            {"rangeproof", dark::rangeproof_to_json(output.rangeproof)}
        });
    }
    stream << send_json.dump(4) << std::endl;

    // connect to messenging service
    auto continue_send = 
        [=, &wallet, &stream](const QString& response)
    {
        final_update_wallet(wallet, response.toStdString(),
            stream, update_balance);
    };

    dark::client_worker_thread *worker = new dark::client_worker_thread(
        tx_id, "final");
    QObject::connect(worker, &dark::client_worker_thread::ready,
        QCoreApplication::instance(), continue_send);
    QObject::connect(worker, &dark::client_worker_thread::finished,
        worker, &QObject::deleteLater);
    worker->start();

    // Now do the actual send
    client.send(send_json.dump());

    // wait for server to broadcast ID of change output back
    stream << "Waiting for response back" << std::endl;
}

void receive_money(dark::wallet& wallet, dark::message_client& client,
    std::string response_string, std::ostream& stream,
    update_balance_callback update_balance)
{
    auto response = json::parse(response_string);
    stream << "Received transaction: " << response.dump(4) << std::endl;

    auto tx = dark::transaction_from_json(response);
    uint64_t amount = response["amount"].get<uint64_t>();
    const uint32_t tx_id = response["tx"]["id"].get<uint32_t>();

    bcs::ec_compressed witness_1_point;
    bool rc = bcs::decode_base16(witness_1_point, response["witness_1"]);
    BITCOIN_ASSERT(rc);
    const bcs::ec_point witness_1 = witness_1_point;
    stream << "Witness 1: "
        << bcs::encode_base16(witness_1.point()) << std::endl;

    const auto salt = keys_map[tx_id];
    const auto witness_2 = salt * bcs::ec_point::G;
    stream << "Witness 2: "
        << bcs::encode_base16(witness_2.point()) << std::endl;
    const auto combined_witness = witness_1 + witness_2;

    // create new output
    auto output = assign_output(amount, stream);

    wallet.insert(output.point, output.secret, amount);

    // compute excess
    const auto excess_secret = output.secret;
    const auto excess = excess_secret * bcs::ec_point::G;
    // compute signature
    const auto signature = dark::sign(excess_secret, salt, combined_witness);
    BITCOIN_ASSERT(dark::verify(signature, excess, combined_witness));

    // verify signature for correctness
    stream << "  (R, s) = ("
        << bcs::encode_base16(tx.kernel.signature.witness.point()) << ", "
        << bcs::encode_base16(tx.kernel.signature.response.secret())
        << ")" << std::endl;
    stream << "  P = "
        << bcs::encode_base16(tx.kernel.excess.point()) << std::endl;
    stream << "  R_1 = "
        << bcs::encode_base16(witness_1.point()) << std::endl;
    stream << "  R_2 = "
        << bcs::encode_base16(witness_2.point()) << std::endl;
    stream << "  R = "
        << bcs::encode_base16(combined_witness.point()) << std::endl;
    BITCOIN_ASSERT(dark::verify(tx.kernel.signature, tx.kernel.excess,
        combined_witness));
    
    // combine excess and signature
    tx.kernel.excess += excess;
    tx.kernel.signature = dark::aggregate(tx.kernel.signature, signature);
    BITCOIN_ASSERT(dark::verify(tx.kernel.signature, tx.kernel.excess,
        combined_witness));
    BITCOIN_ASSERT(dark::verify(tx.kernel.signature, tx.kernel.excess));

    // Modify transaction
    tx.outputs.push_back(dark::transaction_output{
        output.point,
        output.rangeproof
    });

    // broadcast completed tx to server
    json send_json = {
        {"command", "broadcast"},
        {"tx", {
            {"id", tx_id},
            {"inputs", json::array()},
            {"outputs", json::array()},
            {"kernel", {
                {"fee", tx.kernel.fee},
                {"excess", bcs::encode_base16(tx.kernel.excess.point())},
                {"signature", {
                    {"witness", bcs::encode_base16(
                        tx.kernel.signature.witness.point())},
                    {"response", bcs::encode_base16(
                        tx.kernel.signature.response.secret())}
                }}
        }}}}
    };
    for (const auto input: tx.inputs)
    {
        send_json["tx"]["inputs"].push_back(input);
    }
    for (const auto& output: tx.outputs)
    {
        send_json["tx"]["outputs"].push_back({
            {"output", bcs::encode_base16(output.output.point())},
            {"rangeproof", dark::rangeproof_to_json(output.rangeproof)}
        });
    }

    // verify outputs and inputs
    bcs::ec_point re_excess;
    bool is_init = false;
    for (const auto& output: tx.outputs)
    {
        if (!is_init)
        {
            is_init = true;
            re_excess = output.output;
        }
        else
            re_excess += output.output;
    }
    dark::blockchain_client chain;
    for (const auto input: tx.inputs)
    {
        BITCOIN_ASSERT(input < chain.count());
        BITCOIN_ASSERT(chain.exists(input));
        auto result = chain.get(input);
        re_excess -= result.point;
    }
    BITCOIN_ASSERT(tx.kernel.excess == re_excess);

    // connect to messenging service
    auto final_receive = 
        [=, &wallet, &stream](const QString& response_string)
    {
        final_update_wallet(wallet, response_string.toStdString(),
            stream, update_balance);
    };

    dark::client_worker_thread *worker = new dark::client_worker_thread(
        tx_id, "final");
    QObject::connect(worker, &dark::client_worker_thread::ready,
        QCoreApplication::instance(), final_receive);
    QObject::connect(worker, &dark::client_worker_thread::finished,
        worker, &QObject::deleteLater);
    worker->start();

    // Now do the actual send
    client.send(send_json.dump());

    // wait for server to broadcast ID of our output back
}

void show_help()
{
    std::cout << "Usage: darktech [OPTION]" << std::endl;
    std::cout << std::endl;
    std::cout << "  -h, --help\tshow this help text" << std::endl;
    std::cout << "  -w, --write POINT\twrite ec compressed point" << std::endl;
    std::cout << "  -r, --read\tread all the blocks" << std::endl;
    std::cout << "  -d, --delete INDEX\tdelete block" << std::endl;
    std::cout << "  --c1 NUM\tcalculate point #1" << std::endl;
    std::cout << "  --c2 NUM\tcalculate point #2" << std::endl;
}

bool write_point(const std::string& point_string)
{
    bcs::ec_compressed point;
    if (!bcs::decode_base16(point, point_string))
    {
        std::cerr << "Error loading point." << std::endl;
        return false;
    }

    //dark::blockchain chain;
    dark::blockchain_client chain;
    auto index = chain.put(point);
    std::cout << "Allocated #" << index << std::endl;
    return true;
}

void read_all()
{
    //dark::blockchain chain;
    dark::blockchain_client chain;
    const auto chain_count = chain.count();
    for (size_t i = 0; i < chain_count; ++i)
    {
        if (!chain.exists(i))
            continue;
        auto result = chain.get(i);
        std::cout << "#" << i << " "
            << bcs::encode_base16(result.point) << " "
            << std::asctime(std::localtime(&result.time)) << std::endl;
    }
}

void set_commit_table(QTableWidget* table)
{
    table->clear();
    table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    table->setRowCount(0);

    dark::blockchain_client chain;
    const auto chain_count = chain.count();
    for (size_t i = 0; i < chain_count; ++i)
    {
        if (!chain.exists(i))
            continue;
        const size_t index = table->rowCount();
        table->setRowCount(index + 1);

        auto result = chain.get(i);
        QString point_string = QString::fromStdString(
            bcs::encode_base16(result.point));
        QTableWidgetItem *point_item = new QTableWidgetItem(point_string);
        table->setItem(index, 0, point_item);

        QDateTime time = QDateTime::fromSecsSinceEpoch(result.time);
        QTableWidgetItem *time_item = new QTableWidgetItem(
            time.toString("HH:mm ddd d MMM yy"));
        table->setItem(index, 1, time_item);

        std::cout << "#" << i << " "
            << bcs::encode_base16(result.point) << " "
            << std::asctime(std::localtime(&result.time)) << std::endl;
    }
}

bool remove_point(size_t index)
{
    //dark::blockchain chain;
    dark::blockchain_client chain;
    if (index >= chain.count())
    {
        std::cerr << "Error removing invalid index." << std::endl;
        return false;
    }
    if (!chain.exists(index))
    {
        std::cerr << "Error already deleted. Doing nothing." << std::endl;
        return false;
    }
    chain.remove(index);
    return true;
}

class gui_logger_buffer
  : public std::streambuf
{
public:
    gui_logger_buffer(std::streambuf& real_buffer)
      : real_buffer_(real_buffer)
    {
    }

    void set_output_log(QTextEdit* output_log)
    {
        output_log_ = output_log;
    }
protected:
    virtual int overflow(int c)
    {
        if (output_log_)
            line_.append(c);
        real_buffer_.sputc(c);
        return c;
    }

    int sync()
    {
        if (output_log_)
        {
            output_log_->setText(output_log_->toPlainText() + line_);
            line_.clear();
        }
        return 0;
    }
private:
    std::streambuf& real_buffer_;
    QString line_;
    QTextEdit* output_log_ = nullptr;
};

int main(int argc, char** argv)
{
    cxxopts::Options options("darktech", "dark polytechnology");
    options.add_options()
        ("h,help", "Show program help")
        ("write", "Write point",  cxxopts::value<std::string>())
        ("w,wallet", "Path to wallet",  cxxopts::value<std::string>())
        ("u,username", "Username",  cxxopts::value<std::string>())
        ("dest", "Send destination",  cxxopts::value<std::string>())
        ("r,read", "Read all points")
        ("d,delete", "Delete row", cxxopts::value<size_t>())
        ("c1", "Calculate point #1", cxxopts::value<uint32_t>())
        ("c2", "Calculate point #2", cxxopts::value<uint32_t>())
        ("b,balance", "Show balance")
        ("a,add", "Add fake output", cxxopts::value<uint64_t>())
        ("server", "Run blockchain server")
    ;
    auto result = options.parse(argc, argv);

    std::string wallet_path = "wallet.db";
    if (result.count("wallet"))
        wallet_path = result["wallet"].as<std::string>();

    std::string username = "harry";
    if (result.count("username"))
        username = result["username"].as<std::string>();

    gui_logger_buffer buffer(*std::cout.rdbuf());
    std::ostream stream(&buffer);

    auto show_cerr = [](const char* message)
    {
        std::cerr << "Error: " << message << std::endl;
    };

    if (result.count("help"))
    {
        show_help();
        return 1;
    }
    else if (result.count("write"))
    {
        return write_point(result["write"].as<std::string>()) ? 0 : -1;
    }
    else if (result.count("read"))
    {
        read_all();
        return 0;
    }
    else if (result.count("delete"))
    {
        return remove_point(result["delete"].as<size_t>()) ? 0 : -1;
    }
    else if (result.count("c1"))
    {
        if (!result.count("c2"))
        {
            std::cerr << "Error second argument required" << std::endl;
            return -1;
        }
        uint32_t value1 = result["c1"].as<uint32_t>();
        uint32_t value2 = result["c2"].as<uint32_t>();
        calc(value1, value2);
        return 0;
    }
    else if (result.count("balance"))
    {
        dark::wallet wallet(wallet_path);
        show_balance(wallet);
        return 0;
    }
    else if (result.count("add"))
    {
        dark::wallet wallet(wallet_path);
        uint64_t value = result["add"].as<uint64_t>();
        add_output(wallet, value);
        return 0;
    }
    else if (result.count("server"))
    {
        dark::blockchain_server server;
        auto& chain = server.chain();

        std::thread thread([&chain]
        {
            dark::message_server server(chain);
            server.start();
        });
        thread.detach();

        server.start();
        return 0;
    }

    QApplication app(argc, argv);
    QMainWindow *window = new QMainWindow;
    Ui::darkwindow ui;
    ui.setupUi(window);

    dark::wallet wallet(wallet_path);
    dark::message_client client;

    buffer.set_output_log(ui.output_log);

    auto popup_error = [window](const char* message)
    {
        QMessageBox::critical(window, "Dark Wallet", message);
    };

    auto update_balance = [&wallet, &ui]
    {
        set_commit_table(ui.commitment_table);
        do_update_balance(wallet, ui.balance_label);
    };

    QObject::connect(ui.send_button, &QPushButton::clicked, [&]()
    {
        auto amount_string = ui.amount_lineedit->text().toStdString();
        uint64_t amount;
        if (amount_string.empty() ||
            !bcs::decode_base10(amount, amount_string, 8))
        {
            popup_error("Unable to decode amount. Try again.");
            return;
        }
        auto destination = ui.destination_lineedit->text().toStdString();
        if (destination.empty())
        {
            popup_error("Missing destination");
            return;
        }
        send_money(username, wallet, client,
            destination, amount, stream, popup_error, update_balance);
    });

    auto pre_receive_tx = 
        [&wallet, &stream, &client](const QString& response_string)
    {
        auto response = json::parse(response_string.toStdString());
        auto tx_id = response["tx"]["id"].get<uint32_t>();
        const auto salt = dark::new_key();
        keys_map[tx_id] = salt;
        const auto witness = salt * bcs::ec_point::G;
        json send_json = {
            {"command", "request_send_reply"},
            {"tx", {
                {"id", tx_id}
            }},
            {"witness_i", bcs::encode_base16(witness.point())}
        };
        stream << "Sending our key: " << send_json.dump(4) << std::endl;
        client.send(send_json.dump());
    };

    dark::listen_worker_thread *preworker = new dark::listen_worker_thread(
        username, "request_send");
    QObject::connect(preworker, &dark::listen_worker_thread::ready,
        QCoreApplication::instance(), pre_receive_tx);
    QObject::connect(preworker, &dark::listen_worker_thread::finished,
        preworker, &QObject::deleteLater);
    preworker->start();

    auto receive_tx = 
        [&wallet, &client, &stream, update_balance](const QString& response)
    {
        receive_money(wallet, client, response.toStdString(),
            stream, update_balance);
    };

    dark::listen_worker_thread *worker = new dark::listen_worker_thread(
        username, "send");
    QObject::connect(worker, &dark::listen_worker_thread::ready,
        QCoreApplication::instance(), receive_tx);
    QObject::connect(worker, &dark::listen_worker_thread::finished,
        worker, &QObject::deleteLater);
    worker->start();

    update_balance();

    stream << "Username: " << username << std::endl;

    window->show();
    return app.exec();
}

