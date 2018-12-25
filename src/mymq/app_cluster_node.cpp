#include <mymq/chat_room.hpp>
#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/raft_message.hpp>
#include <mymq/cluster_manager.hpp>
#include <mymq/cluster_node.hpp>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]){

    
    const char* ips[][2] = {
        {"localhost", "13"},
        {"localhost", "14"},
        {"localhost", "15"},
        {"localhost", "16"},
        {"localhost", "17"}
    };
    size_t ips_length  = 5;

    const char* mq_port[5] = {
        "9000",
        "9001",
        "9002",
        "9003",
        "9004"
    };

    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: cluster_node current_id\n";
            return 1;
        }
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);

        size_t choice = std::atoi(argv[1]);
        tcp::endpoint this_endpoint;
        std::vector<std::pair<int, tcp::resolver::results_type>> others;
        for(size_t i = 0; i < ips_length; ++i){
            if(i == choice){
                this_endpoint = tcp::endpoint(tcp::v4(), std::atoi(ips[i][1]));
            }
            else{
                others.push_back({i, resolver.resolve(ips[i][0], ips[i][1])});
            }
        }
        //ClusterManager cluster(io_context, this_endpoint, others);
        //cluster.start();
        
        auto client_facing_endpoint = tcp::endpoint(tcp::v4(), std::atoi(mq_port[choice]));
        ClusterNode cluster(client_facing_endpoint, choice, ips_length, io_context, this_endpoint, others);
        
        io_context.run();

    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }



    return 0;
}


