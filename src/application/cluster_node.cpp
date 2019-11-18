#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <flowmq/cluster_manager.hpp>
#include <flowmq/cluster_node.hpp>
#include <flowmq/cluster_node_storage.hpp>
#include <flowmq/configuration.hpp>
#include <flowmq/message.hpp>
#include <flowmq/raft_message.hpp>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>

using boost::asio::ip::tcp;

namespace flowmq {

void RunServer(const std::string& config_file, int cur_node) {
  boost::asio::io_context io_context;
  tcp::resolver resolver(io_context);

  // size_t choice = std::atoi(argv[1]);
  std::ifstream in_file(config_file);
  if (!in_file.is_open()) {
    throw std::runtime_error("could not open config file : " + config_file);
  }
  auto config = flowmq::ServerConfigurationLoader::load_config(in_file);

  // Override.
  if (cur_node >= 0) {
    config.current_node = cur_node;
  }

  int cur_node_id = config.current_node;
  FLOWMQ_OUTPUT_FILE("flowmq_log_" + std::to_string(cur_node_id) + ".log");

  tcp::endpoint this_endpoint;
  std::vector<std::pair<int, tcp::resolver::results_type>> others;
  for (size_t i = 0; i < config.server_nodes.size(); ++i) {
    if (std::get<0>(config.server_nodes[i]) == config.current_node) {
      this_endpoint =
          tcp::endpoint(tcp::v4(), stoi(std::get<2>(config.server_nodes[i])));
    } else {
      others.push_back(
          {i, resolver.resolve(std::get<1>(config.server_nodes[i]),
                               std::get<2>(config.server_nodes[i]))});
    }
  }
  auto client_facing_endpoint = tcp::endpoint(
      tcp::v4(), stoi(std::get<3>(config.server_nodes[cur_node_id])));

  // construct network managers: cluster_manager and client_manager
  std::unique_ptr<ClusterManagerInterface> cluster_manager_p(
      new ClusterManager(io_context, this_endpoint, others));
  std::unique_ptr<ClientManagerInterface> client_manager_p(
      new ClientManager(io_context, client_facing_endpoint));

  // construct cluster_master
  ClusterMaster master(std::move(cluster_manager_p),
                       std::move(client_manager_p), config);

  assert(config.partitions_ids.size() > 0);

  std::list<boost::asio::io_context> node_contexts;
  for (auto partition_id : config.partitions_ids) {
    // add partition 0
    // construct cluster_node
    std::unique_ptr<flowmq::ClusterNodeStorageInterface> storage_p(
        new flowmq::ClusterNodeStorage(partition_id, cur_node_id));

    // {partition_id, node_id, total_nodes}
    ClusterNodeConfig node_config(
        {partition_id, cur_node_id,
         static_cast<int>(config.server_nodes.size())});
    node_contexts.emplace_back();
    std::unique_ptr<ClusterNode> node_p(new ClusterNode(
        node_config, node_contexts.back(), &master, std::move(storage_p)));

    // add cluster_node to cluster_master and start
    master.add_cluster_node(node_config.partition_id, node_config.node_id,
                            std::move(node_p));
  }

  // run the context
  std::list<std::thread> threads;
  for (auto& context : node_contexts) {
    threads.emplace_back([&]() { context.run(); });
  }
  io_context.run();
  for (auto& t : threads) {
    t.join();
  }
}

}  // namespace flowmq

int main(int argc, char* argv[]) {
  try {
    if (argc < 2) {
      std::cerr
          << "Usage: cluster_node [config_file_path] [override current_node]\n";
      return 1;
    }

    int cur_node = -1;
    if (argc > 2) {
      cur_node = atoi(argv[2]);
    }

    flowmq::RunServer(argv[1], cur_node);
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

