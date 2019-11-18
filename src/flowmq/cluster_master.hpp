#pragma once

#include <flowmq/basic_types.h>

#include <flowmq/client_manager.hpp>
#include <flowmq/cluster_manager.hpp>
#include <flowmq/cluster_node.hpp>
#include <flowmq/configuration.hpp>

namespace flowmq {

class ClusterNode;

// Cluster Master that manages network connections
// and several cluster nodes.
class ClusterMaster {
 public:
  // will start the cluster_manager and client_manager upon construction
  ClusterMaster(std::unique_ptr<ClusterManagerInterface> cluster_manager_p,
                std::unique_ptr<ClientManagerInterface> client_manager_p,
                const ServerConfiguration& server_config);

  void add_cluster_node(int partition_id, int node_id,
                        std::unique_ptr<ClusterNode> cluster_node);

  // handles all incoming messages from the cluster
  // This trivially dispatches to the correct parition node.
  void message_handler(const Message& msg);
  void client_disconnected(int client_id);

  ClusterManagerInterface* cluster_manager();

  ClientManagerInterface* client_manager();

  std::pair<std::string, std::string> get_address_for_node(int node_id);

 private:
  std::map<PartitionIdType, std::unique_ptr<ClusterNode>> nodes_;
  std::unique_ptr<ClusterManagerInterface> cluster_manager_p_;
  std::unique_ptr<ClientManagerInterface> client_manager_p_;

  // copy of the server config in order to redirect client to
  // correct leader ip address.
  ServerConfiguration server_config_;
};

}  // namespace flowmq
