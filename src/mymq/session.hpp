#pragma once 

#include <iostream>
#include <queue>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>

using boost::asio::ip::tcp;

// This takes an established socket and owns it by moving semantics. 
// Exposes APIs for writing and reading messages, and registering message handler.
// All write and read operations are async.
//
// When session is disconnected, it would trigger the 
// disconnected handler once. There is no API to 
// reopen this session inplace. Whoever owns the disconnected session
// should consider reopening another new session to replace it.
//
class Session : public std::enable_shared_from_this<Session>
{

    public:

        enum Status {
            CONNECTED = 0, 
            DISCONNECTED = 1, // disconnected state
            CLOSED = 2 
        };

        typedef std::function<void(const Message& msg)> ReadHandler;
        typedef std::function<void()> DisconnectHandler;

        Session(tcp::socket&& socket):
            socket_(std::move(socket)),
            status_(CONNECTED)
    {
    }

        void read_msg(){

            read_header();

        }

        //register read message handler
        void register_handler(const ReadHandler& handler){
            msg_handler_ = handler;
        }

        //register disconnected handler
        void register_disconnect_handler(const DisconnectHandler&  fun){
            disconnected_handler_ = fun;
        }

        void write_message(const Message& msg){
            auto self(shared_from_this());
            boost::system::error_code error;
            //the message will be copied and managed by a shared pointer
            std::shared_ptr<Message> msg_copy = std::make_shared<Message>(msg);
            boost::asio::async_write(socket_, boost::asio::buffer(msg.header(), msg.header_length() + msg.body_length()), 
                    [msg_copy, this, self](boost::system::error_code error, std::size_t){
                    if(!error) return;
                    std::cout << "ERROR write failed! error code "<< error << ' '  
                    << std::string(msg_copy -> body(), msg_copy -> body_length()) << '\n';
                    disconneted();
                    });
        }

        void start_read(){
            read_msg();
        }

        void close(){
            status_ = CLOSED;
            auto self(shared_from_this());
            boost::asio::post(socket_.get_io_context(), [this, self]() { socket_.close(); });
        }

        Status get_status() const{
            return status_;
        }

    private:

        void read_header(){

            auto self(shared_from_this());
            boost::asio::async_read(socket_, boost::asio::buffer(
                        read_message_buffer_.header(), read_message_buffer_.header_length()), 
                    [this, self](boost::system::error_code error, std::size_t /*length*/){

                    if (!error){

                        // this is inovked in the io_context thread, read_message_buffer is invoked
                        // without any synchronization, if multiple threads are enable for 
                        // the associated io_context, application must manage the synchronization properly.
                        read_message_buffer_.decode_length();
                        read_body();
    
                    }
                    else if (error){
                        std::cout << "Error while reading message: " << error
                        << " Maybe socket was closed \n";
                        disconneted();
                        return;
                    }
                    });

        }

        void read_body(){

            auto self(shared_from_this());
            //std::cout << "reading message size " << read_message_buffer_.body_length() << '\n';
            boost::asio::async_read(socket_, boost::asio::buffer(
                        read_message_buffer_.body(), read_message_buffer_.body_length()), 
                    [this, self](boost::system::error_code error, std::size_t /*length*/){

                    if (!error){
                        msg_handler_(read_message_buffer_);
                    }
                    else{
                        std::cout << "Error while reading message: " << error
                        << " Maybe socket was closed \n";
                        disconneted();
                        return;
                    }

                    read_msg();
                    });
        }

        void disconneted(){
            // only trigger if it's not already disconnected
            if(status_ == DISCONNECTED) return;
            status_ = DISCONNECTED;
            disconnected_handler_();
        }

        tcp::socket socket_;
        Message read_message_buffer_;
        ReadHandler msg_handler_;
        std::function<void()> disconnected_handler_;
        Status status_;

};





