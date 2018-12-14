#pragma once 

#include <iostream>
#include <map>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/session.hpp>

using boost::asio::ip::tcp;


// Manages connections with other nodes in the cluster. 
// Does following things:
//     Internally retries connection with cluster nodes periodically.
//     Exposes API to send messages to all nodes.
//     API to send specific message to one node in the cluster.
//     Register handler to process messages from other nodes.
class ClusterManager{

        public:
            using ReadHandler = std::function<void(const Message&)>;

        ClusterManager(boost::asio::io_context& io_context, 
                const tcp::endpoint& endpoint,  // this instance endpoint
                const std::vector<std::pair<int, tcp::resolver::results_type>>& endpoints // other nodes
                ):
            io_context_(io_context),
            acceptor_(io_context, endpoint),
            incoming_sessions_count_(0),
            endpoints_(endpoints)
    {
    }

        void start(){
            std::cout << "end points " << endpoints_.size() << '\n';
            accept_new_connection();
            for(auto& p : endpoints_){
                endpoint_id_map_[p.first] = p.second;
            }
            for(auto& p : endpoints_){
                connect(p.first);
            }
        }

        using SessionPtr = std::shared_ptr<Session>;

        void register_handler(const ReadHandler& handler){
            handler_ = handler;
        }

        void broad_cast(const Message& msg){
            std::cout << "current outoing connections: " << outgoing_sessions_.size() << '\n';
            for(auto& session : outgoing_sessions_){
                session.second -> write_message(msg);
            }
        }

    private:

        // connect would be triggered, causing ever increasing number of retries scheduled.
        void connect(int endpoint_id){
            if(outgoing_sessions_.count(endpoint_id) 
                    && outgoing_sessions_[endpoint_id] -> get_status() == Session::CONNECTED){
                return;
            }

            std::cout << "connecting to " << endpoint_id << "... \n";
            auto socket = std::make_shared<tcp::socket>(io_context_);
            boost::asio::async_connect(*socket, endpoint_id_map_[endpoint_id], [this, socket, endpoint_id](
                        const boost::system::error_code& ec,
                        tcp::endpoint
                        ){

                    if(!ec){
                    std::cout << endpoint_id << " connected\n";

                    auto session = std::make_shared<Session>(std::move(*socket));
                    session -> register_handler([](const Message& msg){
                            std::cout << "#] " << std::string(msg.body(), msg.body_length()) << '\n';
                            });

                    session -> register_disconnect_handler([this, endpoint_id](){
                            std::cout << "Remote session closed, exiting ..." ;
                            auto timer = std::make_shared<boost::asio::deadline_timer>(io_context_);
                            timer -> expires_from_now(boost::posix_time::seconds(2));
                            timer -> async_wait([timer, endpoint_id, this](boost::system::error_code const&){
                                    connect(endpoint_id);
                                    });                    
                            });

                    session -> start_read();
                    outgoing_sessions_[endpoint_id] = session;

                    }
                    else{

                        std::time_t result = std::time(nullptr);
                        std::cout << result << " ERROR endpoint not up, retrying ...\n";
                        auto timer = std::make_shared<boost::asio::deadline_timer>(io_context_);
                        timer -> expires_from_now(boost::posix_time::seconds(2));
                        timer -> async_wait([timer, endpoint_id, this](boost::system::error_code const&){
                                connect(endpoint_id);
                                });                    
                    }

            });
        }


        void accept_new_connection(){
            acceptor_.async_accept([this](boost::system::error_code error, tcp::socket socket){

                    if(!error){

                    std::cout << "got new connection! current connections including this is " 
                    << incoming_sessions_.size() + 1 << '\n';
                    
                    auto new_session = std::make_shared<Session>(std::move(socket));

                    new_session -> register_handler([](const Message& msg){
                            std::cout << "#] " << std::string(msg.body(), msg.body_length()) << '\n';
                            });

                    int id = incoming_sessions_count_++;

                    new_session -> register_disconnect_handler([this, id](){
                            std::cout << "Removing session " << id << " from chat room!\n";
                            auto to_remove = incoming_sessions_.find(id);
                            if(to_remove != incoming_sessions_.end()){
                                incoming_sessions_.erase(incoming_sessions_.find(id));
                            }
                            else{
                                std::cout << "Session " << id << " already removed!\n";
                            }
                            });
                    
                    new_session -> start_read();
                    incoming_sessions_[id] = new_session;

                    }
                    else{
                    std::cout << "ERROR: error while accepting new connection : " << error << '\n';
                    }

                    accept_new_connection();

                    });
        }

        bool running_;

        boost::asio::io_context& io_context_;
        tcp::acceptor acceptor_;
        std::map<int, SessionPtr> incoming_sessions_;
        int incoming_sessions_count_;
        std::vector<std::pair<int, tcp::resolver::results_type>> endpoints_;
        std::map<int, tcp::resolver::results_type> endpoint_id_map_;
        std::map<int, SessionPtr> outgoing_sessions_;
        ReadHandler handler_;

};


