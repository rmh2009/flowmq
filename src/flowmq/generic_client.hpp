#pragma once 

#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <flowmq/message.hpp>
#include <flowmq/session.hpp>

namespace flowmq{

class GenericClient 
{
    public:

        using SessionPtr = std::shared_ptr<Session>;
        using Handler = std::function<void(const Message&)>;

        GenericClient(boost::asio::io_context& io_context, 
                const tcp::resolver::results_type& endpoints);

        void start();

        void reset_endpoint(const tcp::resolver::results_type& endpoints);

        void write_message(Message msg);
        ~GenericClient();

        void stop();

        bool running() const ;

        void register_handler(std::function<void(const Message& msg)> handler);

    private:
        void connect(
                boost::asio::io_context& io_context, 
                const tcp::resolver::results_type& endpoints
                );

        bool running_;
        SessionPtr session_;
        Handler handler_;
        boost::asio::io_context& io_context_;
        tcp::resolver::results_type endpoints_;

};

} // namespace flowmq
