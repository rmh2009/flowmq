#pragma once 

#include <iostream>
#include <map>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/session.hpp>

using boost::asio::ip::tcp;

class ChatRoom{

    public:

        ChatRoom(boost::asio::io_context& io_context, const tcp::endpoint& endpoint):
            acceptor_(io_context, endpoint),
            sessions_count_(0)
    {
        accept_new_connection();
    }

        using SessionPtr = std::shared_ptr<Session>;

    private:

        void accept_new_connection(){
            acceptor_.async_accept([this](boost::system::error_code error, tcp::socket socket){

                    if(!error){

                    std::cout << "got new connection! current connections including this is " 
                    << sessions_.size() + 1 << '\n';
                    
                    auto new_session = std::make_shared<Session>(std::move(socket));

                    new_session -> register_handler([this](const Message& msg){

                            for(auto& session : sessions_){
                            session.second -> write_message(msg);
                            }
                            });

                    int id = sessions_count_++;

                    new_session -> register_disconnect_handler([this, id](){
                            std::cout << "Removing session " << id << " from chat room!\n";
                            auto to_remove = sessions_.find(id);
                            if(to_remove != sessions_.end()){
                                sessions_.erase(sessions_.find(id));
                            }
                            else{
                                std::cout << "Session " << id << " already removed!\n";
                            }
                            });
                    
                    new_session -> start_read();
                    sessions_[id] = new_session;

                    }
                    else{
                    std::cout << "ERROR: error while accepting new connection : " << error << '\n';
                    }

                    accept_new_connection();

                    });
        }

        tcp::acceptor acceptor_;
        std::map<int, SessionPtr> sessions_;
        int sessions_count_;

};
