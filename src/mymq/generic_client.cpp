#include <mymq/generic_client.hpp>

namespace flowmq {

GenericClient::GenericClient(boost::asio::io_context& io_context, 
        const tcp::resolver::results_type& endpoints):
    running_(true),
    io_context_(io_context),
    endpoints_(endpoints)
{
}

void GenericClient::start(){
    connect(io_context_, endpoints_);
}

void GenericClient::write_message(Message msg){
    session_ -> write_message(std::move(msg));
}
GenericClient::~GenericClient(){
    session_ -> close();
}

void GenericClient::stop(){
    session_ -> close();
}

bool GenericClient::running() const {
    return running_;
}

void GenericClient::register_handler(std::function<void(const Message& msg)> handler){
    handler_ = std::move(handler);
}

void GenericClient::connect(
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




} // namespace flowmq
