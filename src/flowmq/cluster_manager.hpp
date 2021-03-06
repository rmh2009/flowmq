#pragma once

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <flowmq/message.hpp>
#include <flowmq/session.hpp>
#include <functional>
#include <iostream>
#include <map>

using boost::asio::ip::tcp;

namespace flowmq {

// Manages connections with other nodes in the cluster.
// Mainly used in the ClusterNode class
// Does following things:
//     Internally retries connection with cluster nodes periodically.
//     Exposes API to send messages to all nodes.
//     API to send specific message to one node in the cluster.
//     Register handler to process messages from other nodes.
class ClusterManagerInterface {
 public:
  using ReadHandler = std::function<void(const Message&)>;
  using SessionPtr = std::shared_ptr<Session>;

  virtual void start() = 0;

  virtual void register_handler(const ReadHandler& handler) = 0;

  virtual void broad_cast(Message msg) = 0;

  virtual void write_message(int endpoint_id, Message msg) = 0;

  virtual ~ClusterManagerInterface() {}
};

class ClusterManager : public ClusterManagerInterface {
 public:
  using ReadHandler = std::function<void(const Message&)>;
  using SessionPtr = std::shared_ptr<Session>;

  ClusterManager(boost::asio::io_context& io_context,
                 const tcp::endpoint& endpoint,  // this instance endpoint
                 const std::vector<std::pair<int, tcp::resolver::results_type>>&
                     endpoints  // other nodes
  );

  void start() override;

  void register_handler(const ReadHandler& handler) override {
    handler_ = handler;
  }

  void broad_cast(Message msg) override;

  void write_message(int endpoint_id, Message msg) override;

 private:
  // connect, with retry logic if disconnected
  void connect(int endpoint_id);

  void accept_new_connection();

  boost::asio::io_context& io_context_;
  tcp::acceptor acceptor_;
  std::map<int, SessionPtr> incoming_sessions_;
  int incoming_sessions_count_;
  std::vector<std::pair<int, tcp::resolver::results_type>> endpoints_;
  std::map<int, tcp::resolver::results_type> endpoint_id_map_;
  std::map<int, SessionPtr> outgoing_sessions_;
  ReadHandler handler_;
};

}  // namespace flowmq
