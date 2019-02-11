#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <flowmq/generic_client.hpp>
#include <flowmq/message.hpp>
#include <flowmq/raft_message.hpp>
#include <flowmq_client/simple_client.hpp>

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
            std::cerr << "Usage: chat_client <host> <port> <partition>\n";
            return 1;
        }

        boost::asio::io_context io_context;
        //io_context.run() will stop once there is no items in the queue!

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(argv[1], argv[2]);
        int partition_id = std::stoi(argv[3]);
        std::vector<int> message_ids; 

        // 1. test send and receive three messages
        flowmq::SimpleClient client(partition_id, io_context, endpoints);

        std::thread t([&io_context](){  
                io_context.run(); 
                });

        client.register_handler([&message_ids](std::string msg, int message_id){
                LOG_INFO << "Got message " << msg;
                message_ids.push_back(message_id);
                });
        client.start();
        std::cout << "client started \n";

        std::string line;

        client.open_queue_sync("test_queue", 0);
        sleep_some_time();

        // consume all pending messages for a clean start
        for(auto id : message_ids){
            client.commit_message(id);
        }
        message_ids.clear();

        // start test
        std::cout << "sending messages to queue \n";
        client.send_message("test1");
        client.send_message("test2");
        client.send_message("test3");
        sleep_some_time();
        if(message_ids.size() != 3){
            std::cout <<"ERROR! did not receive all messages\n";
            return 1;
        }

        // commit one of them
        client.commit_message(message_ids[0]);
        sleep_some_time();
        client.stop();
        std::cout << "client stopped \n";
        message_ids.clear();

        // 2. restart, test that we receive only two messages
        client.start();
        client.open_queue_sync("test_queue", 0);
        std::cout << "client restarted \n";
        sleep_some_time(1);

        if(message_ids.size() != 2){
            std::cout <<"ERROR! did not receive all messages, existing messages received " << message_ids.size()  << "\n";
            return 1;
        }
        //commit the remaining ones
        client.commit_message(message_ids[0]);
        client.commit_message(message_ids[1]);
        sleep_some_time();
        client.stop();

        message_ids.clear();

        // 3. Restart, test that we receive none
        client.start();
        client.open_queue_sync("test_queue", 0);
        sleep_some_time(1);

        if(message_ids.size() != 0){
            std::cout <<"ERROR! commit failed! received messages after committing all previous messages\n";
            return 1;
        }
        client.stop();
        std::cout << "\n ***** SUCCESS! *****\n";

        io_context.stop();
        t.join();

    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }


    return 0;
}
