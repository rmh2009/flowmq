#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <chrono>

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

        flowmq::SimpleClient client(partition_id, io_context, endpoints);

        std::thread t([&io_context](){  
                io_context.run(); 
                });

        int test_count = 10000;
        std::chrono::time_point<std::chrono::system_clock>
            start_time = std::chrono::system_clock::now();
        std::chrono::time_point<std::chrono::system_clock>
            end_time = std::chrono::system_clock::now();
        
        client.register_handler([&](std::string, int message_id){
                //LOG_INFO << "Got message " << msg;
                message_ids.push_back(message_id);
                if(message_ids.size() == (size_t)test_count){
                end_time = std::chrono::system_clock::now();
                }
                });
        client.start();
        std::cout << "client started \n";

        client.open_queue_sync("test_queue", 0);
        sleep_some_time(3);

        // consume all pending messages for a clean start
        for(auto id : message_ids){
            client.commit_message(id);
        }
        message_ids.clear();

        // Start test
        start_time = std::chrono::system_clock::now();
        std::cout << "sending " << test_count << " messages to queue \n";
        for(int i = 0; i < test_count; ++i){
            std::cout << "sent " << i << '\n';
            client.send_message(std::string("test") + std::to_string(i));
        }
        sleep_some_time(10);
        if(message_ids.size() != (size_t)test_count){
            std::cout <<"ERROR! did not receive all messages\n";
            return 1;
        }

        // Commit the remaining ones
        for(auto id : message_ids){
            std::cout << "committing " << id << '\n';
            client.commit_message(id);
        }
        sleep_some_time(3);
        client.stop();

        message_ids.clear();

        std::cout << "\n ***** SUCCESS! *****\n";

        auto duration = end_time - start_time;
        std::cout << "Sent and received " << test_count
            << " messages in time " << duration.count() << '\n';
        std::cout << "Throughput = " <<
           static_cast<double>(test_count) / duration.count() * 1000000 << '\n';
        io_context.stop();
        t.join();

    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }


    return 0;
}
