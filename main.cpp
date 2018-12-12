#include <bitcoin/bitcoin.hpp>

#include <boost/optional.hpp>
#include <boost/range/irange.hpp>
#include <cxxopts.hpp>
#include <QDateTime>
#include <QMessageBox>
#include <QThread>
#include <nlohmann/json.hpp>
#include <dark/blockchain_client.hpp>
#include <dark/blockchain_server.hpp>
#include <dark/message_client.hpp>
#include <dark/transaction.hpp>
#include <dark/utility.hpp>
#include <dark/wallet.hpp>
#include "ui_darkwallet.h"

using json = nlohmann::json;

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
        bc::ec_scalar(value_1) * bc::ec_point::G +
        bc::ec_scalar(value_2) * dark::ec_point_H;
    std::cout << bc::encode_base16(result.point()) << std::endl;
    std::cout << std::endl;

    dark::blockchain chain;
    auto index = chain.put(result);
    std::cout << "Allocated #" << index << std::endl;
}

void show_balance(dark::wallet& wallet)
{
    std::cout << wallet.balance() << std::endl;
}

void update_balance(dark::wallet& wallet, QLabel* balance_label)
{
    auto balance_string = QString::fromStdString(
        bc::encode_base10(wallet.balance(), 8));
    balance_label->setText(balance_string);
}

struct assign_output_result
{
    bc::ec_scalar secret;
    bc::ec_point point;
    dark::transaction_rangeproof rangeproof;
};

assign_output_result assign_output(uint64_t value, std::ostream& stream)
{
    stream << "assign_output(" << value << ")" << std::endl;

    constexpr size_t proofsize = 64;
    typedef std::array<bc::ec_scalar, proofsize> subkeys_list;

    subkeys_list subkeys;
    auto secret = bc::ec_scalar::zero;

    // Create private key by summing 64 sub private keys
    // The 64 subkeys will be used for constructing the rangeproof
    for (auto& subkey: subkeys)
    {
        subkey = dark::new_key();
        secret += subkey;
    }
    BITCOIN_ASSERT(secret);
    stream << "secret: " << bc::encode_base16(secret.secret()) << std::endl;

    auto point =
        secret * bc::ec_point::G + bc::ec_scalar(value) * dark::ec_point_H;

    stream << "point: " << bc::encode_base16(point.point()) << std::endl;

    // [d_1 G + v_(0|1) H] + [d_2 G + v_(0|2) H] + [d_3 G + v_(0|4) H] + ...
    dark::transaction_rangeproof rangeproof;
    rangeproof.commitments.reserve(proofsize);
    // Used for making the signature
    bc::key_rings rangeproof_rings;
    bc::secret_list rangeproof_secrets, rangeproof_salts;
    uint64_t value_checker = 0;
    for (size_t i = 0; i < proofsize; ++i)
    {
        BITCOIN_ASSERT(i < subkeys.size());
        const auto& subkey = subkeys[i];

        // v = 0
        const auto public_key = subkey * bc::ec_point::G;
        // v = 2^i
        uint64_t value_2i = std::pow(2, i);
        const auto value_point = bc::ec_scalar(value_2i) * dark::ec_point_H;

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
    bc::ec_compressed result;
    bc::ec_sum(result, rangeproof.commitments);
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

        const auto key = secret * bc::ec_point::G;
        BITCOIN_ASSERT(key.point() == ring[0] || key.point() == ring[1]);
    }

    bool rc = bc::sign(rangeproof.signature, rangeproof_secrets,
        rangeproof_rings, bc::null_hash, rangeproof_salts);
    BITCOIN_ASSERT(rc);

    BITCOIN_ASSERT(rangeproof.commitments.size() == proofsize);
    bc::key_rings test_rings;
    for (size_t i = 0; i < proofsize; ++i)
    {
        const auto& commitment = rangeproof.commitments[i];
        const uint64_t value_2i = std::pow(2, i);
        test_rings.push_back({
            commitment,
            commitment - bc::ec_scalar(value_2i) * dark::ec_point_H });
    }

    // Verify rangeproof
    rc = bc::verify(test_rings, bc::null_hash, rangeproof.signature);
    BITCOIN_ASSERT(rc);

    stream << "Rangeproof checks out." << std::endl;

    return { secret, point, rangeproof };
}

void add_output(dark::wallet& wallet, uint64_t value)
{
    // Create private key
    bc::ec_secret secret;
    bc::pseudo_random::fill(secret);

    auto value_scalar = bc::ec_scalar(value);

    std::cout << "secret: " << bc::encode_base16(secret) << std::endl;
    std::cout << "scalar: " << bc::encode_base16(value_scalar.secret())
        << std::endl;

    auto point = secret * bc::ec_point::G + value_scalar * dark::ec_point_H;

    std::cout << "point: " << bc::encode_base16(point.point()) << std::endl;

    // Allocate index
    // Add to blockchain
    //dark::blockchain chain;
    dark::blockchain_client chain;
    auto index = chain.put(point);

    // Add to wallet
    wallet.insert(index, point, secret, value);
}

template <typename ShowErrorFunction>
void continue_send_money(const std::string& username, dark::wallet& wallet,
    const std::string& destination, uint64_t amount,
    std::ostream& stream, ShowErrorFunction show_error);

template <typename ShowErrorFunction>
bool send_money(const std::string& username,
    dark::wallet& wallet, dark::message_client& client,
    const std::string& destination, uint64_t amount,
    std::ostream& stream, ShowErrorFunction show_error)
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

    auto selected = wallet.select_outputs(amount);
    stream << "Selected:";
    for (const auto& row: selected)
        stream << " " << row.index;
    stream << std::endl;

    dark::transaction tx;
    for (const auto& row: selected)
        tx.inputs.push_back(row.index);

    typedef boost::optional<assign_output_result> optional_output;
    optional_output change_output;

    BITCOIN_ASSERT(amount <= balance);
    // If we have remaining coins after sending then compute change output
    if (amount < balance)
    {
        // calculate change amount
        auto change_amount = balance - amount;
        BITCOIN_ASSERT(amount + change_amount == balance);
        stream << "Change: " << change_amount << std::endl;
        // Create change output
        // Create 64 private keys, which are used for the rangeproof
        change_output = assign_output(change_amount, stream);
        // Modify transaction
        tx.outputs.push_back(dark::transaction_output{
            change_output->point,
            change_output->rangeproof
        });
    }

    // compute excess
    auto excess_secret = bc::ec_scalar::zero;
    for (const auto& row: selected)
        excess_secret -= row.key;

    if (change_output)
        excess_secret += change_output->secret;

    tx.kernel.excess = excess_secret * bc::ec_point::G;
    stream << "Excess: "
        << bc::encode_base16(tx.kernel.excess.point()) << std::endl;

    // compute signature
    tx.kernel.signature = dark::sign(excess_secret);
    BITCOIN_ASSERT(dark::verify(tx.kernel.signature, tx.kernel.excess));
    stream << "Signature computed and verified" << std::endl;

    // Create json to send
    // send tx, amount with command
    const auto tx_id = dark::random_uint();
    json send_json = {
        {"command", "send"},
        {"tx", {
            {"id", tx_id},
            {"destination", destination},
            {"inputs", json::array()},
            {"outputs", json::array()},
            {"kernel", {
                {"fee", tx.kernel.fee},
                {"excess", bc::encode_base16(tx.kernel.excess.point())},
                {"signature", {
                    {"witness", bc::encode_base16(
                        tx.kernel.signature.witness.point())},
                    {"response", bc::encode_base16(
                        tx.kernel.signature.response.secret())}
                }}
        }}}},
        {"amount", amount}
    };
    for (const auto input: tx.inputs)
    {
        send_json["tx"]["inputs"].push_back(input);
    }
    for (const auto& output: tx.outputs)
    {
        send_json["tx"]["outputs"].push_back({
            {"output", bc::encode_base16(output.output.point())}
        });
    }
    stream << send_json.dump(4) << std::endl;

    // connect to messenging service
    auto continue_send = 
        [=, &wallet, &stream](const QString& response)
    {
        continue_send_money(username, wallet, destination, amount,
            response.toStdString(), stream, show_error);
    };

    dark::client_worker_thread *worker = new dark::client_worker_thread(
        tx_id, "send");
    QObject::connect(worker, &dark::client_worker_thread::ready,
        QCoreApplication::instance(), continue_send);
    QObject::connect(worker, &dark::client_worker_thread::finished,
        worker, &QObject::deleteLater);
    worker->start();

    // Now do the actual send
    client.send(send_json.dump());

    // wait for server to broadcast ID of change output back
    stream << "Waiting for response back" << std::endl;

    return true;
}

template <typename ShowErrorFunction>
void continue_send_money(const std::string& username, dark::wallet& wallet,
    const std::string& destination, uint64_t amount,
    const std::string& response_string,
    std::ostream& stream, ShowErrorFunction show_error)
{
    auto response = json::parse(response_string);
    stream << "Received response: " << response.dump(4) << std::endl;

    // Add to wallet
    //wallet.insert(index, point, secret, value);
}

void receive_money(dark::wallet& wallet)
{
    // connect to messenging service
    // wait for tx, amount with command
    // create new output
    // compute excess
    // compute signature

    // combine excess and signature
    // broadcast completed tx to server

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
    bc::ec_compressed point;
    if (!bc::decode_base16(point, point_string))
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
    for (auto i: boost::irange(chain.count()))
    {
        if (!chain.exists(i))
            continue;
        auto result = chain.get(i);
        std::cout << "#" << i << " "
            << bc::encode_base16(result.point) << " "
            << std::asctime(std::localtime(&result.time)) << std::endl;
    }
}

void set_commit_table(QTableWidget* table)
{
    table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    table->setRowCount(0);

    dark::blockchain_client chain;
    for (auto i: boost::irange(chain.count()))
    {
        if (!chain.exists(i))
            continue;
        const size_t index = table->rowCount();
        table->setRowCount(index + 1);

        auto result = chain.get(i);
        QString point_string = QString::fromStdString(
            bc::encode_base16(result.point));
        QTableWidgetItem *point_item = new QTableWidgetItem(point_string);
        table->setItem(index, 0, point_item);

        QDateTime time = QDateTime::fromSecsSinceEpoch(result.time);
        QTableWidgetItem *time_item = new QTableWidgetItem(
            time.toString("HH:mm ddd d MMM yy"));
        table->setItem(index, 1, time_item);

        std::cout << "#" << i << " "
            << bc::encode_base16(result.point) << " "
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
        ("receive", "Receive funds")
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
    else if (result.count("receive"))
    {
        dark::wallet wallet(wallet_path);
        receive_money(wallet);
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
        server.run();
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

    QObject::connect(ui.send_button, &QPushButton::clicked, [&]()
    {
        auto amount_string = ui.amount_lineedit->text().toStdString();
        uint64_t amount;
        if (amount_string.empty() ||
            !bc::decode_base10(amount, amount_string, 8))
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
            destination, amount, stream, popup_error);
    });

    auto receive_tx = 
        [&stream](const QString& response)
    {
        stream << "Received transaction: "
            << response.toStdString() << std::endl;
    };

    dark::listen_worker_thread *worker = new dark::listen_worker_thread(
        username);
    QObject::connect(worker, &dark::listen_worker_thread::ready,
        QCoreApplication::instance(), receive_tx);
    QObject::connect(worker, &dark::listen_worker_thread::finished,
        worker, &QObject::deleteLater);
    worker->start();

    set_commit_table(ui.commitment_table);
    update_balance(wallet, ui.balance_label);

    window->show();
    return app.exec();
}

