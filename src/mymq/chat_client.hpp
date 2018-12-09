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
                const tcp::resolver::results_type& endpoints)
        {
            connect(io_context, endpoints);
        }
        void write_message(const Message& msg){
            session_ -> write_message(msg);
        }
        ~ChatClient(){
            session_ -> close();
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
                    std::cout << "Obtained message: " << msg.data() << '\n';
                    });
            session_ -> start_read();
        }

        SessionPtr session_;

};
