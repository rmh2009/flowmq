#include <flowmq/session.hpp>
#include <flowmq/logging.hpp>

namespace flowmq{

Session::Session(tcp::socket&& socket):
    socket_(std::move(socket)),
    status_(CONNECTED)
{
}

void Session::write_message(std::shared_ptr<Message>msg_copy){
    auto self(shared_from_this());
    boost::system::error_code error;
    boost::asio::async_write(socket_, boost::asio::buffer(
                msg_copy->header(), 
                msg_copy->header_length() + msg_copy->body_length()), 
            [msg_copy, this, self](boost::system::error_code error, std::size_t){
            if(!error) {
            //LOG_INFO << "INFO session.cpp successfully wrote message!\n";
            return;
            }
            LOG_ERROR << "write failed! error code "<< error << ' '  
            << std::string(msg_copy -> body(), msg_copy -> body_length()) ;
            disconneted();
            });
}

void Session::close(){
    status_ = CLOSED;
    auto self(shared_from_this());
    boost::asio::post(socket_.get_io_context(), [this, self]() { socket_.close(); });
}

void Session::read_header(){

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
            LOG_ERROR << "Error while reading message: " << error
            << " Maybe socket was closed \n";
            disconneted();
            return;
            }
            });
}
void Session::read_body(){

    auto self(shared_from_this());
    boost::asio::async_read(socket_, boost::asio::buffer(
                read_message_buffer_.body(), read_message_buffer_.body_length()), 
            [this, self](boost::system::error_code error, std::size_t /*length*/){

            if (!error){
            //LOG_INFO << "session : obtained message : " << 
            //std::string(read_message_buffer_.body(), read_message_buffer_.body_length()) << '\n';

            msg_handler_(read_message_buffer_);
            }
            else{
            LOG_ERROR << "Error while reading message: " << error
            << " Maybe socket was closed \n";
            disconneted();
            return;
            }

            read_msg();
            });
}

void Session::disconneted(){
    // only trigger if it's not already disconnected
    if(status_ == DISCONNECTED) return;
    status_ = DISCONNECTED;
    disconnected_handler_();
}

} // namespace flowmq
