#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <flowmq/basic_types.hpp>
#include <flowmq/generic_client.hpp>
#include <flowmq/message.hpp>
#include <flowmq/raft_message.hpp>
#include <flowmq_client/simple_client.hpp>
#include <functional>
#include <iostream>

void sleep_some_time() { usleep(1000000); }

void sleep_some_time(int seconds) { usleep(seconds * 1000000); }

int main(int argc, char* argv[]) {
  try {
    if (argc != 4) {
      std::cerr << "Usage: chat_client <host> <port> <partition>\n";
      return 1;
    }

    boost::asio::io_context io_context;
    // io_context.run() will stop once there is no items in the queue!

    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(argv[1], argv[2]);
    int partition_id = std::stoi(argv[3]);
    std::vector<flowmq::MessageIdType> message_ids;

    flowmq::SimpleClient client(partition_id, io_context, endpoints);

    std::thread t([&io_context]() { io_context.run(); });

    client.register_handler(
        [&message_ids](std::string msg, flowmq::MessageIdType message_id) {
          LOG_INFO << "Got message " << msg;
          std::cout << "Got message " << message_id << '\n';
          message_ids.push_back(message_id);
        });
    client.start();
    std::cout << "client started \n";

    client.open_queue_sync("test_queue", 0);
    sleep_some_time(3);

    // consume all pending messages for a clean start
    for (auto id : message_ids) {
      client.commit_message(id);
    }
    message_ids.clear();

    // 1. start test
    int test_count = 10;
    std::cout << "sending " << test_count << " messages to queue \n";
    for (int i = 0; i < test_count; ++i) {
      std::cout << "sent " << i << '\n';
      client.send_message(std::string("test") + std::to_string(i));
    }
    sleep_some_time(10);
    if (message_ids.size() != (size_t)test_count) {
      std::cout << "ERROR! did not receive all messages, messages received: "
                << message_ids.size() << "\n";
      return 1;
    }

    // 2. commit one of them
    std::cout << "Committing one message: " << message_ids[0] << '\n';
    client.commit_message(message_ids[0]);
    sleep_some_time(2);
    client.stop();
    std::cout << "client stopped \n";
    message_ids.clear();

    // 3. restart, test that we receive the rest of the messages.
    client.start();
    client.open_queue_sync("test_queue", 0);
    std::cout << "client restarted \n";
    sleep_some_time(1);

    size_t expected_messages = (size_t)test_count - 1;
    if (message_ids.size() != expected_messages) {
      std::cout << "ERROR! Number of messages receive does not match, messages "
                   "received "
                << message_ids.size() << " Expecting " << expected_messages
                << "\n";
      return 1;
    }
    // 4.commit the remaining ones
    for (auto id : message_ids) {
      std::cout << "committing " << id << '\n';
      client.commit_message(id);
    }
    sleep_some_time(3);
    client.stop();

    message_ids.clear();

    // 5. Restart, test that we receive none
    client.start();
    client.open_queue_sync("test_queue", 0);
    sleep_some_time(3);

    if (message_ids.size() != 0) {
      std::cout << "ERROR! commit failed! received messages after committing "
                   "all previous messages\n";
      return 1;
    }
    client.stop();
    std::cout << "\n ***** SUCCESS! *****\n";

    io_context.stop();
    t.join();

  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
