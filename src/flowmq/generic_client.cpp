#include <flowmq/generic_client.hpp>
#include <flowmq/logging.hpp>

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

void GenericClient::reset_endpoint(const tcp::resolver::results_type& endpoints){
    endpoints_ = endpoints;
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
    LOG_INFO << "connecting ... \n";
    tcp::socket socket(io_context);
    boost::asio::connect(socket, endpoints);

    session_ = std::make_shared<Session>(std::move(socket));
    session_ -> register_handler([this](const Message& msg){
            handler_(msg);
            });

    session_-> register_disconnect_handler([this](){
            LOG_INFO << "Remote session closed, exiting ...\n" ;
            running_ = false;
            });

    session_ -> start_read();

    LOG_INFO << "connected\n";
}




} // namespace flowmq
