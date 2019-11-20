#pragma once

#include <flowmq/basic_types.hpp>

#include <functional>
#include <iostream>
#include <tuple>
#include <vector>

namespace flowmq {

struct ServerConfiguration {
  // Tuple of <node_id, host_ip, server_port, client_port> representing the
  // server host and ports
  typedef std::tuple<int, std::string, std::string, std::string> ServerPoint;

  int current_node;

  // for each node id.
  std::vector<ServerPoint> server_nodes;
  std::vector<PartitionIdType> partitions_ids;
};

// Load config file
// Example config file:
//
// current_node = 0
//
// id = 0
// host = localhost
// cluster_port = 12
// client_port = 9000
//
// id = 2
// ...

struct ServerConfigurationLoader {
  static ServerConfiguration load_config(std::istream& in_stream);

 private:
  // get key and value from a string, return true on success
  static bool get_key_value(const std::string& line, std::string* key,
                            std::string* value);

  // strip whie spaces
  static std::string strip(const std::string& str);
};

}  // namespace flowmq
