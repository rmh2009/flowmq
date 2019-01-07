#include <mymq/chat_client.hpp>
#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/raft_message.hpp>

using boost::asio::ip::tcp;


void open_queue(ChatClient& client, const std::string& queue_name, int mode){

        RaftMessage raft_msg;
        raft_msg.loadClientOpenQueueRequest(ClientOpenQueueRequestType{
                mode,
                queue_name
                });
        client.write_message(Message(raft_msg.serialize()));
}

void commit_message(ChatClient& client, int message_id){


    RaftMessage raft_msg;
    raft_msg.loadClientCommitMessageRequest(ClientCommitMessageType{
            message_id
            });
    client.write_message(Message(raft_msg.serialize()));

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
        std::thread t([&io_context](){ io_context.run(); });
        tcp::resolver resolver(io_context);

        auto endpoints = resolver.resolve(argv[1], argv[2]);
        std::string user_name(argv[3]);
        ChatClient client(io_context, endpoints);
        client.register_handler([](const Message& msg){
                std::cout << "Received message : " << std::string(msg.body(), msg.body_length())
                << '\n';
                });

        client.start();
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
            raft_msg.loadClientPutMessageRequest(ClientPutMessageType{line});
            client.write_message(Message(raft_msg.serialize()));
        }
        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }


    return 0;
}
