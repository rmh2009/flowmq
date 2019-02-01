#include <iostream>
#include <fstream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <flowmq/message.hpp>
#include <flowmq/raft_message.hpp>
#include <flowmq/cluster_manager.hpp>
#include <flowmq/cluster_node.hpp>
#include <flowmq/configuration.hpp>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]){

    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: cluster_node [config_file_path] [override current_node]\n";
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
        if(argc > 2){
            config.current_node = atoi(argv[2]);
        }
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


