#include <flowmq/cluster_master.hpp>

namespace flowmq {

ClusterMaster::ClusterMaster(
    std::unique_ptr<ClusterManagerInterface> cluster_manager_p,
    std::unique_ptr<ClientManagerInterface> client_manager_p,
    const ServerConfiguration& server_config)
    : cluster_manager_p_(std::move(cluster_manager_p)),
      client_manager_p_(std::move(client_manager_p)),
      server_config_(server_config) {
  cluster_manager_p_->register_handler(
      std::bind(&ClusterMaster::message_handler, this, std::placeholders::_1));
  cluster_manager_p_->start();
  client_manager_p_->register_handler(
      std::bind(&ClusterMaster::message_handler, this, std::placeholders::_1));
  client_manager_p_->register_disconnect_handler(std::bind(
      &ClusterMaster::client_disconnected, this, std::placeholders::_1));
  client_manager_p_->start();
}

void ClusterMaster::add_cluster_node(
    PartitionIdType partition_id, int node_id, std::unique_ptr<ClusterNode> cluster_node) {
  LOG_INFO << "adding node with partition " << partition_id << " and node_id "
           << node_id << " to cluster master\n";
  nodes_[partition_id] = std::move(cluster_node);
}

// handles all incoming messages from the cluster
// This trivially dispatches to the correct parition node.
void ClusterMaster::message_handler(const Message& msg) {
  // TODO optimize this to avoid deserialize twice
  RaftMessage raft_msg = RaftMessage::deserialize_from_message(msg);
  if (nodes_.find(raft_msg.partition_id()) == nodes_.end()) {
    LOG_ERROR << "Parition id " << raft_msg.partition_id() << " not found!\n";
    assert(false);
  }

  // dispatch to correct partition
  nodes_[raft_msg.partition_id()]->message_handler(msg);
}

void ClusterMaster::client_disconnected(int client_id) {
  // dispatch to all partitions
  for (auto& p : nodes_) {
    p.second->consumer_disconnected(client_id);
  }
}

// TODO since this could be invoked by multiple cluster_nodes,
// each node could have its own processing thread, confirm that
// this is thread safe, i.e. cluster_manager and client_manager
// needs to be thread safe.
ClusterManagerInterface* ClusterMaster::cluster_manager() {
  return cluster_manager_p_.get();
}

// Similar to above
ClientManagerInterface* ClusterMaster::client_manager() {
  return client_manager_p_.get();
}

std::pair<std::string, std::string> ClusterMaster::get_address_for_node(
    int node_id) {
  for (auto& p : server_config_.server_nodes) {
    if (std::get<0>(p) == node_id) {
      return {std::get<1>(p), std::get<3>(p)};
    }
  }
  return {"", ""};
}

std::map<PartitionIdType, ClusterNode*> ClusterMaster::get_nodes() const {
 std::map<PartitionIdType, ClusterNode*> nodes; 
 for (const auto& node: nodes_) {
   nodes.insert({node.first, node.second.get()});
 }
 return nodes;
}

}  // namespace flowmq
