#pragma once 

#include <iostream>
#include <queue>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>

using boost::asio::ip::tcp;

// This takes an established socket and 
// owns it by moving semantics. 
// Exposes APIs for writing and reading messages, and registering message handler.
class Session{

    public:

        typedef std::function<void(const Message& msg)> Handler;

        Session(tcp::socket&& socket):
            socket_(std::move(socket))
    {
    }

        void read_msg(){

            boost::asio::async_read(socket_, boost::asio::buffer(
                        read_message_buffer_.data(), read_message_buffer_.length()), 
                    [this](boost::system::error_code error, std::size_t /*length*/){

                    if (!error){
                    msg_handler_(read_message_buffer_);
                    }
                    else if (error){
                    std::cout << "Error while reading message: " << error
                    << " Maybe socket was closed \n";
                    return;
                    }
                    read_msg();
                    });

        }

        void register_handler(const Handler& handler){
            msg_handler_ = handler;
        }

        void write_message(const Message& msg){
            boost::system::error_code error;
            boost::asio::write(socket_, boost::asio::buffer(msg.data(), msg.length()), error);
        }

        void start_read(){
            read_msg();
        }

        void close(){
            boost::asio::post(socket_.get_io_context(), [this]() { socket_.close(); });
        }


    private:

        tcp::socket socket_;
        Message read_message_buffer_;
        Message write_message_buffer_;
        std::queue<Message> write_messages_;
        Handler msg_handler_;

};





