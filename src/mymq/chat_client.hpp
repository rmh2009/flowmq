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
        using Handler = std::function<void(const Message&)>;

        ChatClient(boost::asio::io_context& io_context, 
                const tcp::resolver::results_type& endpoints):
            running_(true),
            io_context_(io_context),
            endpoints_(endpoints)
        {
        }

        void start(){
            connect(io_context_, endpoints_);
        }

        void write_message(const Message& msg){
            session_ -> write_message(msg);
        }
        ~ChatClient(){
            session_ -> close();
        }

        void stop(){
            session_ -> close();
        }

        bool running() const {
            return running_;
        }

        void register_handler(std::function<void(const Message& msg)> handler){
            handler_ = std::move(handler);
        }

    private:
        void connect(
                boost::asio::io_context& io_context, 
                const tcp::resolver::results_type& endpoints
                ){
            std::cout << "connecting ... \n";
            tcp::socket socket(io_context);
            boost::asio::connect(socket, endpoints);

            session_ = std::make_shared<Session>(std::move(socket));
            session_ -> register_handler([this](const Message& msg){
                    handler_(msg);
                    });

            session_-> register_disconnect_handler([this](){
                        std::cout << "Remote session closed, exiting ...\n" ;
                        running_ = false;
                    });
            
            session_ -> start_read();

            std::cout << "connected\n";
        }

        bool running_;
        SessionPtr session_;
        Handler handler_;
        boost::asio::io_context& io_context_;
        tcp::resolver::results_type endpoints_;

};
