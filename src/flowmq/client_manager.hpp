#pragma once 

#include <iostream>
#include <map>
#include <set>
#include <ctime>
#include <random>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <flowmq/message.hpp>
#include <flowmq/session.hpp>

using boost::asio::ip::tcp;

namespace flowmq{

// Manages connection with message queue client on the server side. 
// This class is mainly used in the cluster_node manager.
class ClientManager{

    public:
        using ReadHandler = std::function<void(const Message&)>;
        using ClientDisconnectedHandler = std::function<void(int client_id)>;
        using SessionPtr = std::shared_ptr<Session>;

        ClientManager(
                boost::asio::io_context& io_context, 
                const tcp::endpoint& endpoint  // this instance endpoint
                );

        void start();

        void register_handler(const ReadHandler& handler);

        void register_disconnect_handler(const ClientDisconnectedHandler& handler);

        //int get_next_consumer(){
        //}

        // Returns the client id the message was delivered to. Returns -1 on failure.
        int deliver_one_message_round_robin(Message msg);

        bool has_consumers() const;

    private:
        void accept_new_connection();

        void handle_message(const Message& msg, int client_id);

        boost::asio::io_context& io_context_;
        tcp::acceptor acceptor_;

        std::map<int, SessionPtr> client_sessions_;
        std::vector<int> consumer_client_id_array_;
        int deliver_count_;

        ReadHandler handler_;
        ClientDisconnectedHandler client_disconnected_handler_;
};

} // namespace flowmq




