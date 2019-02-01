#pragma once

#include <iostream>
#include <functional>
#include <vector>
#include <tuple>

namespace flowmq{

struct ServerConfiguration{

    typedef std::tuple<int, std::string, std::string, std::string> ServerPoint;

    int current_node;

    // Tuple of <node_id, host, server_port, client_port> representing the server host and ports 
    // for each node id.
    std::vector<ServerPoint> server_nodes;

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

struct ServerConfigurationLoader{
    static ServerConfiguration load_config(std::istream& in_stream);

    private:

    // get key and value from a string, return true on success
    static bool get_key_value(const std::string& line,
            std::string* key,
            std::string* value);

    // strip whie spaces
    static std::string strip(const std::string& str);
};

} // namespace flowmq
