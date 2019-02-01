#pragma once 

#include <iostream>
#include <queue>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <flowmq/message.hpp>

using boost::asio::ip::tcp;

namespace flowmq{

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

        Session(tcp::socket&& socket);

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

        void write_message(Message msg){
            //the message will be taken and managed by a shared pointer
            write_message(std::make_shared<Message>(std::move(msg)));
        }

        void write_message(std::shared_ptr<Message>msg_copy);

        void start_read(){
            read_msg();
        }

        void close();

        Status get_status() const{
            return status_;
        }

    private:

        void read_header();

        void read_body();

        void disconneted();

        tcp::socket socket_;
        Message read_message_buffer_;
        ReadHandler msg_handler_;
        std::function<void()> disconnected_handler_;
        Status status_;

};



} // namespace flowmq




