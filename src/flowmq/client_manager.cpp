#include <flowmq/client_manager.hpp>
#include <flowmq/logging.hpp>
#include <flowmq/raft_message.hpp>

namespace flowmq {

ClientManager::ClientManager(
    boost::asio::io_context& io_context,
    const tcp::endpoint& endpoint  // this instance endpoint
    )
    : io_context_(io_context),
      acceptor_(io_context_, endpoint),
      deliver_count_() {}

void ClientManager::start() { accept_new_connection(); }

void ClientManager::register_handler(const ReadHandler& handler) {
  handler_ = handler;
}

void ClientManager::register_disconnect_handler(
    const ClientDisconnectedHandler& handler) {
  client_disconnected_handler_ = handler;
}

// Returns the client id the message was delivered to. Returns -1 on failure.
int ClientManager::deliver_one_message_round_robin(PartitionIdType partition_id,
                                                   Message msg) {
  std::unique_lock<std::mutex> lock(mutex_);

  if (consumer_client_id_array_.count(partition_id) == 0) return 1;

  if (consumer_client_id_array_[partition_id].size() == 0) {
    LOG_ERROR << "ERROR! client_manager.cpp no active client not found!\n";
    return -1;
  }

  size_t index_in_array = deliver_count_[partition_id] %
                          consumer_client_id_array_[partition_id].size();
  int client_id = consumer_client_id_array_[partition_id][index_in_array];

  client_sessions_[client_id]->write_message(std::move(msg));
  ++deliver_count_[partition_id];

  return client_id;
}

bool ClientManager::has_consumers() const {
  return consumer_client_id_array_.size() > 0;
}

void ClientManager::accept_new_connection() {
  acceptor_.async_accept([this](boost::system::error_code error,
                                tcp::socket socket) {
    if (!error) {
      LOG_INFO << "got new connection! current connections including this is "
               << client_sessions_.size() + 1 << '\n';

      auto new_session = std::make_shared<Session>(std::move(socket));
      int id = std::rand();

      new_session->register_handler([this, id](const Message& msg) {
        LOG_INFO << "#] " << std::string(msg.body(), msg.body_length()) << '\n';
        handle_message(msg, id);
      });

      new_session->register_disconnect_handler([this, id]() {
        LOG_DEBUG << "Removing session " << id << " from client manager!\n";

        std::unique_lock<std::mutex> lock(mutex_);
        auto to_remove = client_sessions_.find(id);
        if (to_remove != client_sessions_.end()) {
          client_sessions_.erase(client_sessions_.find(id));
        } else {
          LOG_DEBUG << "Session " << id << " already removed!\n";
        }

        // remove disconnected client from all partition listening queues
        for (auto& p : consumer_client_id_array_) {
          auto to_remove_from_array =
              std::find(p.second.begin(), p.second.end(), id);
          if (to_remove_from_array != p.second.end()) {
            p.second.erase(to_remove_from_array);
          } else {
            LOG_DEBUG << "Session " << id
                      << " does not exist in consumer id array!\n";
          }
        }

        client_disconnected_handler_(id);
      });

      new_session->start_read();
      client_sessions_[id] = new_session;

    } else {
      LOG_ERROR << "error while accepting new connection : " << error << '\n';
    }

    accept_new_connection();
  });
}

void ClientManager::handle_message(const Message& msg, int client_id) {
  // TODO optimize this. This is currently deserialized twice, once here, second
  // time in the handler_()
  RaftMessage raft_msg = RaftMessage::deserialize_from_message(msg);

  std::unique_lock<std::mutex> lock(mutex_);

  switch (raft_msg.type()) {
    case RaftMessage::CLIENT_OPEN_QUEUE: {
      const ClientOpenQueueRequestType& req = raft_msg.get_open_queue_request();
      LOG_INFO << "Obtained request to open queue : " << req.DebugString()
               << '\n';
      auto partition_id = raft_msg.partition_id();
      consumer_client_id_array_[partition_id].push_back(client_id);
      // TODO optimization: pass the deserialized raft_msg instead
      handler_(msg);
      break;
    }
    case RaftMessage::CLIENT_PUT_MESSAGE:
    case RaftMessage::CLIENT_COMMIT_MESSAGE:
      // forward to handler (in this case the cluster node)
      handler_(msg);
      break;
    default:
      throw std::runtime_error("unexpeted message type in client manager!");
      break;
  }
}

}  // namespace flowmq

