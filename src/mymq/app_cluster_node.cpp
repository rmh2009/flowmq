#include <mymq/chat_room.hpp>
#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/raft_message.hpp>
#include <mymq/cluster_manager.hpp>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]){

    
    const char* ips[][2] = {
        {"localhost", "13"},
        {"localhost", "14"},
        {"localhost", "15"},
        {"localhost", "16"},
        {"localhost", "17"}
    };
    size_t ips_lenghth  = 5;

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
        for(size_t i = 0; i < ips_lenghth; ++i){
            if(i == choice){
                this_endpoint = tcp::endpoint(tcp::v4(), std::atoi(ips[i][1]));
            }
            else{
                others.push_back({i, resolver.resolve(ips[i][0], ips[i][1])});
            }
        }
        ClusterManager cluster(io_context, this_endpoint, others);

        cluster.start();
        boost::asio::deadline_timer timer(io_context);
        timer.expires_from_now(boost::posix_time::seconds(2));

        std::function<void(boost::system::error_code const&)> fun = 
            [&cluster, choice, &timer, &fun](boost::system::error_code const&){

                RaftMessage raft_message;
                raft_message.loadVoteRequest(0, choice, 100, 200);
                Message msg(raft_message.serialize());
                cluster.broad_cast(msg);
                timer.expires_from_now(boost::posix_time::seconds(2));
                timer.async_wait(fun);
        };

        timer.async_wait(fun);

        io_context.run();

    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }



    return 0;
}


