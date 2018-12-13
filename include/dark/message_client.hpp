#ifndef DARK_MESSAGE_CLIENT_HPP
#define DARK_MESSAGE_CLIENT_HPP

#include <string>
#include <QString>
#include <QThread>
#include <czmq.h>

namespace dark {

class message_client
{
public:
    message_client();
    ~message_client();

    void send(const std::string& message);

    std::string receive();
private:
    zsock_t* sender_socket_ = nullptr;
    zsock_t* receiver_socket_ = nullptr;
};

class client_worker_thread
  : public QThread
{
public:
    client_worker_thread(const uint32_t tx_id, const std::string& command);
private:
    Q_OBJECT
    void run() override;

    const uint32_t tx_id_;
    const std::string command_;
    message_client client_;
signals:
    void ready(const QString &response);
};

class listen_worker_thread
  : public QThread
{
public:
    listen_worker_thread(
        const std::string& username, const std::string& command);
private:
    Q_OBJECT
    void run() override;

    const std::string username_;
    const std::string command_;
    message_client client_;
signals:
    void ready(const QString &response);
};

} // namespace dark

#endif

