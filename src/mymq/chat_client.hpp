#pragma once 

#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/session.hpp>

class ChatClient 
{
    public:

        using SessionPtr = std::shared_ptr<Session>;

        ChatClient(boost::asio::io_context& io_context, 
                const tcp::resolver::results_type& endpoints):
            running_(true)
        {
            connect(io_context, endpoints);
        }
        void write_message(const Message& msg){
            session_ -> write_message(msg);
        }
        ~ChatClient(){
            session_ -> close();
        }

        bool running() const {
            return running_;
        }

    private:
        void connect(
                boost::asio::io_context& io_context, 
                const tcp::resolver::results_type& endpoints
                ){
            std::cout << "connecting ... \n";
            tcp::socket socket(io_context);
            boost::asio::connect(socket, endpoints);
            std::cout << "connected\n";

            session_ = std::make_shared<Session>(std::move(socket));
            session_ -> register_handler([](const Message& msg){
                    std::cout << "#] " << std::string(msg.body(), msg.body_length()) << '\n';
                    });

            session_-> register_disconnect_handler([this](){
                        std::cout << "Remote session closed, exiting ..." ;
                        running_ = false;
                    });
            
            session_ -> start_read();
        }

        bool running_;
        SessionPtr session_;

};
