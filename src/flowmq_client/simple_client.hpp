#pragma once

#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <flowmq/generic_client.hpp>
#include <flowmq/message.hpp>
#include <flowmq/raft_message.hpp>

using boost::asio::ip::tcp;
using flowmq::GenericClient;
using flowmq::RaftMessage;
using flowmq::Message;

namespace flowmq{

// Simple client, only supports connecting to one server node at the same time.
// Automatically redirects to leader when opening a paritition, so the 
// initial node selection doesn't matter.
//
// TODO needs refactoring
class SimpleClient{
    public:
        using Handler = std::function<void(std::string message, int message_id)>;
        static const int RETRIES_LIMIT = 10;

        SimpleClient(long long partition_id, boost::asio::io_context& io_context,
                const tcp::resolver::results_type& endpoints):
            io_context_(io_context),
            generic_client_(io_context, endpoints),
            partition_id_(partition_id),
            stats_timer_(io_context),
            stopped_(false),
            retries_(0)
    {
        // starts stats() immediately, this will prevent io_context from stopping
        stats();
        generic_client_.register_handler(std::bind(&SimpleClient::handle_message, this, std::placeholders::_1));
    }

        void start(){
            stopped_ = false;
            generic_client_.start();
        }

        void stop(){
            stopped_ = true;
            generic_client_.stop();
        }

        int open_queue_sync(const std::string& queue_name, int mode){

            std::unique_lock<std::mutex> lock(mutex_);

            flowmq::ClientOpenQueueRequestType req;
            req.set_open_mode(mode);
            req.set_queue_name(queue_name);
            open_queue_msg_.loadClientOpenQueueRequest(std::move(req));
            open_queue_msg_.set_partition_id(partition_id_);
            generic_client_.write_message(open_queue_msg_.serialize_as_message());

            condition_.wait(lock);
            return retries_ < RETRIES_LIMIT;
        }

        void commit_message(int message_id){

            RaftMessage raft_msg;
            flowmq::ClientCommitMessageType req;
            req.set_message_id(message_id);
            raft_msg.loadClientCommitMessageRequest(std::move(req));
            raft_msg.set_partition_id(partition_id_);
            generic_client_.write_message(raft_msg.serialize_as_message());
        }

        void send_message(const std::string& message){

            RaftMessage raft_msg;
            flowmq::ClientPutMessageType req;
            req.set_message(message);
            raft_msg.loadClientPutMessageRequest(std::move(req));
            raft_msg.set_partition_id(partition_id_);
            generic_client_.write_message(raft_msg.serialize_as_message());
        }

        void register_handler(const Handler& handler){
            handler_ = handler;
        }


    private:

        void stats(){
            stats_timer_.expires_from_now(boost::posix_time::seconds(5));
            stats_timer_.async_wait([this](boost::system::error_code ){
                    std::cout << "stats ... \n";
                    // place holder
                    // also keeps the io_context alive
                    if(!stopped_)stats();
                    });
        }

        void handle_message(const Message& msg){
            std::cout << "Received message : " << RaftMessage::deserialize_from_message(msg).DebugString() << '\n';
            RaftMessage raft_msg = RaftMessage::deserialize(std::string(msg.body(), msg.body_length()));

            if(raft_msg.type() == RaftMessage::CLIENT_OPEN_QUEUE_RESPONSE){
                auto& resp = raft_msg.get_open_queue_response();
                if(resp.status() == RaftMessage::ERROR){
                    if(resp.leader_ip() == "") return;
                    tcp::resolver reslv(io_context_);
                    generic_client_.reset_endpoint(reslv.resolve(
                                resp.leader_ip(), resp.leader_port()));
                    LOG_INFO << "wrong leader, retrying correct leader\n";
                    generic_client_.start();
                    generic_client_.write_message(open_queue_msg_.serialize_as_message());
                }
                else{
                    LOG_INFO << "queue opened \n";
                    open_queue_msg_ = RaftMessage(); //clear
                    condition_.notify_all();
                }
            }

            // dispatch to client handler
            if(raft_msg.type() == RaftMessage::SERVER_SEND_MESSAGE){
                auto& req = raft_msg.get_server_send_essage();
                handler_(req.message(), req.message_id());
            }

        }

        boost::asio::io_context& io_context_;
        GenericClient generic_client_;
        long long partition_id_;
        Handler handler_;
        boost::asio::deadline_timer stats_timer_;
        bool stopped_;

        // ugly hack to support automatic retrying in open_queue_sync()
        std::mutex mutex_;
        std::condition_variable condition_;
        RaftMessage open_queue_msg_;
        int retries_;

};

} // namespace flowmq

