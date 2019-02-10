#pragma once 

#include <iostream>
#include <map>
#include <set>
#include <ctime>
#include <random>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <flowmq/basic_types.h>
#include <flowmq/message.hpp>
#include <flowmq/session.hpp>

using boost::asio::ip::tcp;

namespace flowmq{

// Manages connection with message queue client on the server side. 
// This class is mainly used in the cluster_node manager.
class ClientManagerInterface{

    public:
        using ReadHandler = std::function<void(const Message&)>;
        using ClientDisconnectedHandler = std::function<void(int client_id)>;
        using SessionPtr = std::shared_ptr<Session>;

        virtual void start() = 0;

        virtual void register_handler(const ReadHandler& handler) = 0;

        virtual void register_disconnect_handler(const ClientDisconnectedHandler& handler) = 0;

        // Returns the client id the message was delivered to. Returns -1 on failure.
        virtual int deliver_one_message_round_robin(PartitionIdType partition_id, Message msg) = 0;

        virtual bool has_consumers() const = 0;

        virtual ~ClientManagerInterface(){};
};

class ClientManager : public ClientManagerInterface{

    public:
        using ReadHandler = std::function<void(const Message&)>;
        using ClientDisconnectedHandler = std::function<void(int client_id)>;
        using SessionPtr = std::shared_ptr<Session>;

        ClientManager(
                boost::asio::io_context& io_context, 
                const tcp::endpoint& endpoint  // this instance endpoint
                );

        void start() override;

        void register_handler(const ReadHandler& handler) override;

        void register_disconnect_handler(const ClientDisconnectedHandler& handler) override;

        // Returns the client id the message was delivered to. Returns -1 on failure.
        int deliver_one_message_round_robin(PartitionIdType partition_id, Message msg) override;

        bool has_consumers() const override;

    private:
        void accept_new_connection();

        void handle_message(const Message& msg, int client_id);

        boost::asio::io_context& io_context_;
        tcp::acceptor acceptor_;

        std::map<int, SessionPtr> client_sessions_;
        std::map<PartitionIdType, std::vector<int>> consumer_client_id_array_;
        std::map<PartitionIdType, int> deliver_count_;

        ReadHandler handler_;
        ClientDisconnectedHandler client_disconnected_handler_;
};

} // namespace flowmq




