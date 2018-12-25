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

        std::cout << "client started \n";

        std::thread t([&io_context](){ io_context.run(); });

        std::string line;
        while (client.running() && std::getline(std::cin, line))
        {
            if(line == "open"){ //magic word!
                open_queue(client, "test_queue", 0);
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
