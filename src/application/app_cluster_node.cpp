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
#include <flowmq/cluster_node_storage.hpp>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]){
    using namespace flowmq;
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

        // construct network managers: cluster_manager and client_manager
        std::unique_ptr<ClusterManagerInterface> cluster_manager_p(new 
                ClusterManager(io_context, this_endpoint, others));
        std::unique_ptr<ClientManagerInterface> client_manager_p(new
                ClientManager(io_context, client_facing_endpoint));
        
        // construct cluster_master
        ClusterMaster master(std::move(cluster_manager_p), std::move(client_manager_p));

        // construct cluster_node
        std::unique_ptr<flowmq::ClusterNodeStorageInterface> storage_p(new flowmq::ClusterNodeStorage(0, choice));

        // {partition_id, node_id, total_nodes}
        ClusterNodeConfig node_config({0, choice, static_cast<int>(config.server_nodes.size())}); 
        std::unique_ptr<ClusterNode> node_p(new 
                ClusterNode(node_config, io_context, &master, std::move(storage_p)));

        // add cluster_node to cluster_master and start
        master.add_cluster_node(node_config.partition_id, node_config.node_id, std::move(node_p));

        // run the context
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }



    return 0;
}


