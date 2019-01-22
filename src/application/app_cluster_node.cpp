#include <iostream>
#include <fstream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/raft_message.hpp>
#include <mymq/cluster_manager.hpp>
#include <mymq/cluster_node.hpp>
#include <mymq/configuration.hpp>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]){

    
    //const char* ips[][2] = {
    //    {"localhost", "13"},
    //    {"localhost", "14"},
    //    {"localhost", "15"},
    //    {"localhost", "16"},
    //    {"localhost", "17"}
    //};
    //size_t ips_length  = 5;

    //const char* mq_port[5] = {
    //    "9000",
    //    "9001",
    //    "9002",
    //    "9003",
    //    "9004"
    //};

    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: cluster_node [config_file_path]";
            return 1;
        }
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);

        //size_t choice = std::atoi(argv[1]);
        std::string config_file(argv[1]);
        std::ifstream in_file(config_file);
        if(!in_file.is_open()){
            throw std::runtime_error("could not open config file : " + config_file);
        }
        auto config = flowmq::ServerConfigurationLoader::load_config(in_file);
        int choice = config.current_node;

        tcp::endpoint this_endpoint;
        std::vector<std::pair<int, tcp::resolver::results_type>> others;
        for(size_t i = 0; i < config.server_nodes.size(); ++i){
            if(i == static_cast<size_t>(config.current_node)){
                this_endpoint = tcp::endpoint(tcp::v4(), stoi(std::get<2>(config.server_nodes[i])));
            }
            else{
                others.push_back({i, resolver.resolve(
                            std::get<1>(config.server_nodes[i]), 
                            std::get<2>(config.server_nodes[i]))});
            }
        }
        //ClusterManager cluster(io_context, this_endpoint, others);
        //cluster.start();
        
        auto client_facing_endpoint = tcp::endpoint(tcp::v4(), stoi(std::get<3>(config.server_nodes[choice])));
        flowmq::ClusterNode cluster(client_facing_endpoint, choice, config.server_nodes.size(), io_context, this_endpoint, others);
        
        io_context.run();

    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }



    return 0;
}


