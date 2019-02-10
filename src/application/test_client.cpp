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

class SimpleClient{
    public:
        SimpleClient(long long partition_id):
            partition_id_(partition_id){}

        void open_queue(GenericClient& client, const std::string& queue_name, int mode){

            RaftMessage raft_msg;
            flowmq::ClientOpenQueueRequestType req;
            req.set_open_mode(mode);
            req.set_queue_name(queue_name);
            raft_msg.loadClientOpenQueueRequest(std::move(req));
            raft_msg.set_partition_id(partition_id_);
            client.write_message(raft_msg.serialize_as_message());
        }

        void commit_message(GenericClient& client, int message_id){

            RaftMessage raft_msg;
            flowmq::ClientCommitMessageType req;
            req.set_message_id(message_id);
            raft_msg.loadClientCommitMessageRequest(std::move(req));
            raft_msg.set_partition_id(partition_id_);
            client.write_message(raft_msg.serialize_as_message());
        }

        void send_message(GenericClient& client, const std::string& message){

            RaftMessage raft_msg;
            flowmq::ClientPutMessageType req;
            req.set_message(message);
            raft_msg.loadClientPutMessageRequest(std::move(req));
            raft_msg.set_partition_id(partition_id_);
            client.write_message(raft_msg.serialize_as_message());
        }

    private:

        long long partition_id_;
};

std::shared_ptr<GenericClient> get_new_client(boost::asio::io_context& io_context, const tcp::resolver::results_type& endpoints,
        std::vector<int>& message_ids){

    std::shared_ptr<GenericClient> client = std::make_shared<GenericClient>(io_context, endpoints);
    client -> register_handler([&message_ids](const Message& msg){

            std::cout << "Received message : " << std::string(msg.body(), msg.body_length())
            << '\n';
            RaftMessage raft_msg = RaftMessage::deserialize(std::string(msg.body(), msg.body_length()));

            if(raft_msg.type() == RaftMessage::SERVER_SEND_MESSAGE){
            const flowmq::ServerSendMessageType& send_message_result = raft_msg.get_server_send_essage();
            message_ids.push_back(send_message_result.message_id());
            }

            });
    return client;
}

void sleep_some_time(){
    usleep(1000000);
}

void sleep_some_time(int seconds){
    usleep(seconds * 1000000);
}

int main(int argc, char* argv[]){

    try
    {
        if (argc != 4)
        {
            std::cerr << "Usage: chat_client <host> <port> <user_name>\n";
            return 1;
        }

        boost::asio::io_context io_context;
        bool stop_iocontext = false;
        //io_context.run() will stop once there is no items in the queue!
        std::thread t([&io_context, &stop_iocontext](){  
                io_context.run(); 

                while(!stop_iocontext){
                usleep(500000); 
                std::cout << "restarted io_context\n";
                io_context.restart();
                io_context.run();}
                }

                );

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(argv[1], argv[2]);
        std::string user_name(argv[3]);
        std::vector<int> message_ids; 

        // 1. test send and receive three messages
        auto client = get_new_client(io_context, endpoints, message_ids);
        SimpleClient mqclient(0);
        client -> start();
        std::cout << "client started \n";

        std::string line;

        mqclient.open_queue(*client, "test_queue", 0);

        std::cout << "sending messages to queue \n";
        mqclient.send_message(*client, "test1");
        mqclient.send_message(*client, "test2");
        mqclient.send_message(*client, "test3");

        sleep_some_time();

        if(message_ids.size() != 3){
            std::cout <<"ERROR! did not receive all messages\n";
            return 1;
        }

        // commit one of them
        mqclient.commit_message(*client, message_ids[0]);

        sleep_some_time();
        client -> stop();
        std::cout << "client stopped \n";

        message_ids.clear();

        // 2. restart, test that we receive only two messages
        client = get_new_client(io_context, endpoints, message_ids);
        client -> start();
        sleep_some_time(1);
        mqclient.open_queue(*client, "test_queue", 0);

        std::cout << "client restarted \n";
        sleep_some_time(1);

        if(message_ids.size() != 2){
            std::cout <<"ERROR! did not receive all messages, existing messages received " << message_ids.size()  << "\n";
            return 1;
        }
        //commit the remaining ones
        mqclient.commit_message(*client, message_ids[0]);
        mqclient.commit_message(*client, message_ids[1]);
        client -> stop();

        message_ids.clear();


        // 3. Restart, test that we receive none
        client -> start();
        mqclient.open_queue(*client, "test_queue", 0);

        if(message_ids.size() != 0){
            std::cout <<"ERROR! commit failed! received messages after committing all previous messages\n";
            return 1;
        }
        client -> stop();


        stop_iocontext = true;
        t.join();

        std::cout << "\n ***** SUCCESS! *****\n";
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }


    return 0;
}
