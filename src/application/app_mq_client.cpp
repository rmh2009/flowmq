#include <mymq/chat_client.hpp>
#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/raft_message.hpp>

using boost::asio::ip::tcp;

using flowmq::RaftMessage;
using flowmq::ChatClient;
using flowmq::Message;

void open_queue(ChatClient& client, const std::string& queue_name, int mode){

        RaftMessage raft_msg;
        flowmq::ClientOpenQueueRequestType req;
        req.set_open_mode(mode);
        req.set_queue_name(queue_name);
        raft_msg.loadClientOpenQueueRequest(std::move(req));
        client.write_message(raft_msg.serialize_as_message());
}

void commit_message(ChatClient& client, int message_id){


    RaftMessage raft_msg;
    flowmq::ClientCommitMessageType req;
    req.set_message_id(message_id);
    raft_msg.loadClientCommitMessageRequest(std::move(req));
    client.write_message(raft_msg.serialize_as_message());
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
        tcp::resolver resolver(io_context);

        auto endpoints = resolver.resolve(argv[1], argv[2]);
        std::string user_name(argv[3]);
        ChatClient client(io_context, endpoints);
        client.register_handler([](const Message& msg){
                std::cout << "received message : \n";
                std::cout << RaftMessage::deserialize(std::string(msg.body(), msg.body_length())).DebugString() << '\n';
                });

        client.start();
        // must start client first then start the io_context
        // io_context would stop once there is no job queued, client.start() would 
        // always put a job on the io_context queue to read next message so that 
        // it doesn't stop early.
        std::thread t([&io_context](){ io_context.run(); });
        std::cout << "client started \n";

        std::string line;
        while (client.running() && std::getline(std::cin, line))
        {
            if(line == "open"){ //magic word!
                open_queue(client, "test_queue", 0);
                continue;
            }

            if(line.find("commit") != std::string::npos){
                int msg_id = -1;
                msg_id = std::stoi(line.substr(6, line.size()));
                if(msg_id == -1) continue;
                commit_message(client, msg_id);
                std::cout << "committing message id " << msg_id << '\n';
                continue;
            }

            RaftMessage raft_msg;
            flowmq::ClientPutMessageType req;
            req.set_message(line);
            raft_msg.loadClientPutMessageRequest(std::move(req));
            client.write_message(raft_msg.serialize_as_message());
        }
        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }


    return 0;
}
