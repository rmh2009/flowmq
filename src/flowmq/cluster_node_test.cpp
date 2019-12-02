#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <flowmq/client_manager.hpp>
#include <flowmq/cluster_master.hpp>
#include <flowmq/cluster_node.hpp>
#include <flowmq/configuration.hpp>

namespace flowmq {

class MockClusterManager;

// A mock network class that can transfer messages between several
// MockClusterManager class instances.
//
// This class is thread compatible. The const methods are thread safe. The
// messages will be delivered to the message_handler() method of ClusterNode,
// which is a thread safe method.
class MockNetwork {
 public:
  MockNetwork(const MockNetwork&) = delete;
  MockNetwork& operator=(const MockNetwork&) = delete;

  MockNetwork() {}

  void register_cluster_manager(int id, MockClusterManager* manager) {
    assert(clusters_.find(id) == clusters_.end());
    clusters_.insert({id, manager});
  }

  // Broadcasts a message from id to other nodes.
  void broad_cast_message_from(int id, Message msg) const;

  // Send a message to a specific node id.
  void send_message_to(int id, Message msg) const;

 private:
  std::map<int, MockClusterManager*> clusters_;
};

class MockClusterManager : public ClusterManagerInterface {
 public:
  MockClusterManager(int id, MockNetwork* network)
      : id_(id), network_(network) {
    network_->register_cluster_manager(id_, this);
  }

  void start() override {}

  void register_handler(const ReadHandler& handler) override {
    handler_ = handler;
  }

  void broad_cast(Message msg) override {
    network_->broad_cast_message_from(id_, std::move(msg));
  }

  void write_message(int endpoint_id, Message msg) override {
    network_->send_message_to(endpoint_id, std::move(msg));
  }

  ~MockClusterManager() override {}

  // Use this to send mock message from the network.
  void receive_mock_message(const Message& msg) { handler_(msg); }

 private:
  int id_;
  ReadHandler handler_;
  MockNetwork* network_;
};

void MockNetwork::broad_cast_message_from(int id, Message msg) const {
  for (const auto& cluster : clusters_) {
    if (cluster.first == id) {
      continue;
    }
    cluster.second->receive_mock_message(msg);
  }
}

void MockNetwork::send_message_to(int id, Message msg) const {
  const auto target = clusters_.find(id);
  if (target == clusters_.end()) {
    return;
  }
  target->second->receive_mock_message(msg);
}

// This class does nothing, we don't expect it to be called in this test.
class MockClientManager : public ClientManagerInterface {
 public:
  void start() override {}

  void register_handler(const ReadHandler&) override {}

  void register_disconnect_handler(const ClientDisconnectedHandler&) override {}

  // Returns the client id the message was delivered to. Returns -1 on failure.
  int deliver_one_message_round_robin(PartitionIdType, Message) override {
    return 0;
  }

  bool has_consumers() const override { return true; }

  ~MockClientManager() override{};
};

TEST(TestClusterNode, TestMockNetwork) {
  MockNetwork network;

  std::vector<RaftMessage> messages_0;
  MockClusterManager manager_0(0, &network);
  manager_0.register_handler([&messages_0](const Message& msg) {
    RaftMessage raft_msg;
    raft_msg.deserialize_from_message(msg);
    messages_0.push_back(std::move(raft_msg));
  });
  std::vector<RaftMessage> messages_1;
  MockClusterManager manager_1(1, &network);
  manager_1.register_handler([&messages_1](const Message& msg) {
    RaftMessage raft_msg;
    raft_msg.deserialize_from_message(msg);
    messages_1.push_back(std::move(raft_msg));
  });

  RaftMessage raft_msg;
  AppendEntriesRequestType req;
  raft_msg.loadAppendEntriesRequest(req);
  Message msg;
  raft_msg.serialize_to_message(&msg);
  network.broad_cast_message_from(0, std::move(msg));
  network.broad_cast_message_from(1, std::move(msg));

  ASSERT_EQ(messages_0.size(), 1);
  ASSERT_EQ(messages_1.size(), 1);

  network.send_message_to(0, std::move(msg));
  network.send_message_to(1, std::move(msg));

  ASSERT_EQ(messages_0.size(), 2);
  ASSERT_EQ(messages_1.size(), 2);
}

TEST(TestClusterNode, TestClusterNodeStartup) {
  MockNetwork network;
  boost::asio::io_context io_context;
  ServerConfiguration server_config;

  const auto create_cluster_master_fn = [&io_context, &network, &server_config](
                                            int partition, int cur_node,
                                            int total_nodes) {
    // Create network managers.
    std::unique_ptr<MockClusterManager> manager(
        new MockClusterManager(cur_node, &network));
    std::unique_ptr<MockClientManager> client(new MockClientManager());
    // Create the node master.
    ClusterMaster master(std::move(manager), std::move(client), server_config);

    // Create node and add to master.
    ClusterNodeConfig node_config = {partition, cur_node, total_nodes};
    std::unique_ptr<flowmq::ClusterNodeStorageInterface> storage_p(
        new flowmq::ClusterNodeStorageMock(partition, cur_node));
    std::unique_ptr<ClusterNode> node(new ClusterNode(
        node_config, io_context, &master, std::move(storage_p)));
    master.add_cluster_node(partition, cur_node, std::move(node));
    return master;
  };

  auto master_0 = create_cluster_master_fn(0, 0, 3);
  auto master_1 = create_cluster_master_fn(0, 1, 3);
  auto master_2 = create_cluster_master_fn(0, 2, 3);

  // Stop the masters in some seconds.
  std::thread thread([&io_context]() {
    usleep(3000000);
    io_context.stop();
  });
  io_context.run();

  // Check that exactly one is the leader.
  int leader = -1;
  if (master_0.get_nodes().begin()->second->current_state() ==
      ClusterNode::LEADER) {
    std::cout << "Found leader 0.\n";
    leader += 1;
  }
  if (master_1.get_nodes().begin()->second->current_state() ==
      ClusterNode::LEADER) {
    std::cout << "Found leader 1.\n";
    leader += 1;
  }
  if (master_2.get_nodes().begin()->second->current_state() ==
      ClusterNode::LEADER) {
    std::cout << "Found leader 2.\n";
    leader += 1;
  }
  std::cout << "leader = " << leader << '\n';
  EXPECT_EQ(leader, 0);
  thread.join();
}

}  // namespace flowmq

