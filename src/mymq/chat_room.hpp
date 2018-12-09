#pragma once 

#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/session.hpp>

using boost::asio::ip::tcp;

class ChatRoom{

    public:

        ChatRoom(boost::asio::io_context& io_context, const tcp::endpoint& endpoint):
            acceptor_(io_context, endpoint)
    {
        accept_new_connection();
    }

        using SessionPtr = std::shared_ptr<Session>;

    private:

        void accept_new_connection(){
            acceptor_.async_accept([this](boost::system::error_code error, tcp::socket socket){
                    if(!error){

                    auto new_session = std::make_shared<Session>(std::move(socket));
                    new_session -> register_handler([this](const Message& msg){

                            for(auto& session : sessions_){
                            session -> write_message(msg);
                            }
                            });
                    new_session -> start_read();
                    sessions_.push_back(new_session);

                    }
                    else{
                    std::cout << "ERROR: error while accepting new connection : " << error << '\n';
                    }

                    accept_new_connection();

                    });
        }

        tcp::acceptor acceptor_;
        std::vector<SessionPtr> sessions_;

};
