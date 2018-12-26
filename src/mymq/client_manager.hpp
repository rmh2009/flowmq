#pragma once 

#include <iostream>
#include <map>
#include <set>
#include <ctime>
#include <random>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/raft_message.hpp>
#include <mymq/session.hpp>

using boost::asio::ip::tcp;

class ClientManager{

    public:
        using ReadHandler = std::function<void(const Message&)>;
        using ClientDisconnectedHandler = std::function<void(int client_id)>;
        using SessionPtr = std::shared_ptr<Session>;

        ClientManager(
                boost::asio::io_context& io_context, 
                const tcp::endpoint& endpoint  // this instance endpoint
                ):
            io_context_(io_context),
            acceptor_(io_context, endpoint),
                deliver_count_(0)
    {
    }

        void start(){
            accept_new_connection();
        }

        void register_handler(const ReadHandler& handler){
            handler_ = handler;
        }

        void register_disconnect_handler(const ClientDisconnectedHandler& handler){
            client_disconnected_handler_ = handler;
        }

        //int get_next_consumer(){
        //}

        // Returns the client id the message was delivered to. Returns -1 on failure.
        int deliver_one_message_round_robin(const Message& msg){

            if(consumer_client_id_array_.size() == 0) return -1;

            size_t index_in_array = deliver_count_ % consumer_client_id_array_.size();
            int client_id = consumer_client_id_array_[index_in_array];

            client_sessions_[client_id]->write_message(msg);
            ++deliver_count_;

            return client_id;

        }

        bool has_consumers() const{
            return consumer_client_id_array_.size() > 0;
        }

    private:
        void accept_new_connection(){
            acceptor_.async_accept([this](boost::system::error_code error, tcp::socket socket){

                    if(!error){

                    std::cout << "got new connection! current connections including this is " 
                    << client_sessions_.size() + 1 << '\n';

                    auto new_session = std::make_shared<Session>(std::move(socket));
                    int id = std::rand();

                    new_session -> register_handler([this, id](const Message& msg){
                            std::cout << "#] " << std::string(msg.body(), msg.body_length()) << '\n';
                            handle_message(msg, id);
                            });


                    new_session -> register_disconnect_handler([this, id](){
                            std::cout << "Removing session " << id << " from client manager!\n";
                            auto to_remove = client_sessions_.find(id);
                            if(to_remove != client_sessions_.end()){
                            client_sessions_.erase(client_sessions_.find(id));
                            }
                            else{
                            std::cout << "Session " << id << " already removed!\n";
                            }

                            auto to_remove_from_array = std::find(consumer_client_id_array_.begin(), consumer_client_id_array_.end(), id);
                            if(to_remove_from_array != consumer_client_id_array_.end()){
                            consumer_client_id_array_.erase(to_remove_from_array);
                            }
                            else{
                            std::cout << "Session " << id << " does not exist in consumer id array!\n";
                            }

                            client_disconnected_handler_(id);

                            });

                    new_session -> start_read();
                    client_sessions_[id] = new_session;

                    }
                    else{
                        std::cout << "ERROR: error while accepting new connection : " << error << '\n';
                    }

                    accept_new_connection();

            });
        }

        void handle_message(const Message& msg, int client_id){

            RaftMessage raft_msg = RaftMessage::deserialize(std::string(msg.body(), msg.body_length()));

            switch(raft_msg.type()){
                case RaftMessage::ClientOpenQueue : 
                    {
                    const ClientOpenQueueRequestType& req = raft_msg.get_open_queue_request();
                    std::cout << "Obtained request to open queue : " << req.queue_name << " open mode : " << req.open_mode << '\n';
                    consumer_client_id_array_.push_back(client_id);
                    handler_(msg);
                    break;
                    }
                case RaftMessage::ClientPutMessage:
                case RaftMessage::ClientCommitMessage:
                    //forward to handler (in this case the cluster node)
                    handler_(msg);
                    break;
                default:
                    throw std::runtime_error("unexpeted message type in client manager!");
                    break;
            }
        }

        boost::asio::io_context& io_context_;
        tcp::acceptor acceptor_;

        std::map<int, SessionPtr> client_sessions_;
        std::vector<int> consumer_client_id_array_;
        int deliver_count_;

        ReadHandler handler_;
        ClientDisconnectedHandler client_disconnected_handler_;
};




