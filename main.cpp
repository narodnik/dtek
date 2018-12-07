#include <bitcoin/bitcoin.hpp>

#include <boost/range/irange.hpp>
#include <cxxopts.hpp>
#include <dark/blockchain_client.hpp>
#include <dark/blockchain_server.hpp>
#include <dark/transaction.hpp>
#include <dark/wallet.hpp>
#include <QMessageBox>
#include "ui_darkwallet.h"

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

struct assign_output_result
{
    dark::output_index_type index;
    bc::ec_secret secret;
    bc::ec_point point;
    dark::transaction_rangeproof rangeproof;
};

assign_output_result assign_output(dark::wallet& wallet, uint64_t value)
{
    constexpr size_t proofsize = 64;
    typedef std::array<bc::ec_scalar, proofsize> subkeys_list;

    subkeys_list subkeys;
    auto combined_key = bc::ec_scalar::zero;

    // Create private key by summing 64 sub private keys
    // The 64 subkeys will be used for constructing the rangeproof
    for (auto& subkey: subkeys)
    {
        auto& subkey_secret = subkey.secret();
        bc::pseudo_random::fill(subkey_secret);

        combined_key += subkey;
    }
    const auto& secret = combined_key.secret();

    auto value_secret = bc::ec_scalar::zero.secret();
    auto serial = bc::make_unsafe_serializer(value_secret.end() - 4);
    serial.write_4_bytes_big_endian(value);

    std::cout << "secret: " << bc::encode_base16(secret) << std::endl;
    std::cout << "scalar: " << bc::encode_base16(value_secret)
        << std::endl;

    auto point = secret * bc::ec_point::G + value_secret * dark::ec_point_H;

    std::cout << "point: " << bc::encode_base16(point.point()) << std::endl;

    // Allocate index
    // Add to blockchain
    //dark::blockchain chain;
    dark::blockchain_client chain;
    auto index = chain.put(point);

    // Add to wallet
    wallet.insert(index, point, secret, value);

    // [d_1 G + v_(0|1) H] + [d_2 G + v_(0|2) H] + [d_3 G + v_(0|4) H] + ...
    dark::transaction_rangeproof rangeproof;
    rangeproof.commitments.reserve(proofsize);
    for (size_t i = 1; i <= proofsize; ++i)
    {
        BITCOIN_ASSERT(i < subkeys.size());
        const auto& subkey = subkeys[i];

        const auto public_key = subkey * bc::ec_point::G;
        // v = 0
        rangeproof.commitments.push_back(public_key);
        // v = 2^i
        uint64_t value_2i = std::pow(2, i);

        // 2 keys
    }

    return { index, value_secret, point, rangeproof };
}

void add_output(dark::wallet& wallet, uint64_t value)
{
    // Create private key
    bc::ec_secret secret;
    bc::pseudo_random::fill(secret);

    auto value_secret = bc::ec_scalar::zero.secret();
    auto serial = bc::make_unsafe_serializer(value_secret.end() - 4);
    serial.write_4_bytes_big_endian(value);

    std::cout << "secret: " << bc::encode_base16(secret) << std::endl;
    std::cout << "scalar: " << bc::encode_base16(value_secret)
        << std::endl;

    auto point = secret * bc::ec_point::G + value_secret * dark::ec_point_H;

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
bool send_money(dark::wallet& wallet, uint64_t amount, std::ostream& stream,
    ShowErrorFunction show_error)
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

    stream << "Sending: " << amount << std::endl;

    auto selected = wallet.select_outputs(amount);
    stream << "Selected:";
    for (const auto& row: selected)
        stream << " " << row.index;
    stream << std::endl;

    dark::transaction tx;
    for (const auto& row: selected)
        tx.inputs.push_back(row.index);

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
        auto change_output = assign_output(wallet, change_amount);
        // Modify transaction
        tx.outputs.push_back(dark::transaction_output{
            change_output.point,
            change_output.rangeproof
        });
    }

    // compute excess
    // compute signature

    // connect to messenging service
    // send tx, amount with command

    // wait for server to broadcast ID of change output back

    return true;
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
        auto point = chain.get(i);
        std::cout << "#" << i << " " << bc::encode_base16(point) << std::endl;
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

        auto point = chain.get(i);
        QString point_string = QString::fromStdString(
            bc::encode_base16(point));
        QTableWidgetItem *point_item = new QTableWidgetItem(point_string);
        table->setItem(index, 0, point_item);

        std::cout << "#" << i << " " << bc::encode_base16(point) << std::endl;
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
        ("r,read", "Read all points")
        ("d,delete", "Delete row", cxxopts::value<size_t>())
        ("c1", "Calculate point #1", cxxopts::value<uint32_t>())
        ("c2", "Calculate point #2", cxxopts::value<uint32_t>())
        ("b,balance", "Show balance")
        ("s,send", "Send money", cxxopts::value<uint64_t>())
        ("receive", "Receive funds")
        ("a,add", "Add fake output", cxxopts::value<uint64_t>())
        ("server", "Run blockchain server")
    ;
    auto result = options.parse(argc, argv);

    std::string wallet_path = "wallet.db";
    if (result.count("wallet"))
        wallet_path = result["wallet"].as<std::string>();

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
    else if (result.count("send"))
    {
        dark::wallet wallet(wallet_path);
        uint64_t amount = result["send"].as<uint64_t>();
        return send_money(wallet, amount, stream, show_cerr) ? 0 : -1;
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
        send_money(wallet, amount, stream, popup_error);
    });

    set_commit_table(ui.commitment_table);

    window->show();
    return app.exec();
}

