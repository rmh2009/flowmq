#include <flowmq/configuration.hpp>
#include <flowmq/logging.hpp>

namespace flowmq {

ServerConfiguration ServerConfigurationLoader::load_config(
    std::istream& in_stream) {
  ServerConfiguration config;

  // std::ifstream config_file(config_filename);
  // if(!config_file.is_open()){
  //    throw std::runtime_error("unable to open config file");
  //}

  std::string id;
  std::string host;
  std::string cluster_port;
  std::string client_port;

  std::string line;
  while (std::getline(in_stream, line)) {
    std::string key;
    std::string value;

    if (line.empty() || line[0] == '#') {
      continue;
    }

    if (!get_key_value(line, &key, &value)) {
      continue;
    }

    LOG_INFO << "key : " << key << ", value : " << value << '\n';

    if (key == "current_node") {
      config.current_node = stoi(value);
    } else if (key == "id") {
      id = value;
    } else if (key == "host") {
      host = value;
    } else if (key == "cluster_port") {
      cluster_port = value;
    } else if (key == "client_port") {
      client_port = value;
    } else if (key == "partition_id") {
      config.partitions_ids.push_back(std::stoll(value));
    } else {
      throw std::logic_error(
          "Failured parsing Config file, unknown key name : " + key);
    }

    if (!id.empty() && !host.empty() && !cluster_port.empty() &&
        !client_port.empty()) {
      config.server_nodes.push_back(
          std::make_tuple(stoi(id), host, cluster_port, client_port));
      id = "";
      host = "";
      cluster_port = "";
      client_port = "";
    }
  }

  return config;
}

// get key and value from a string, return true on success
bool ServerConfigurationLoader::get_key_value(const std::string& line,
                                              std::string* key,
                                              std::string* value) {
  size_t del = line.find_first_of('=');
  if (del == std::string::npos) {
    return false;
  }
  *key = strip(line.substr(0, del));
  *value = strip(line.substr(del + 1));
  return (!key->empty() && !value->empty());
}

// strip whie spaces
std::string ServerConfigurationLoader::strip(const std::string& str) {
  size_t start = str.find_first_not_of(' ');
  size_t end = str.find_last_not_of(' ');

  return str.substr(start, end - start + 1);
}

}  // namespace flowmq

