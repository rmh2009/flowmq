#include <flowmq/raft_message.hpp>

int main() {
  auto fun = [](const RaftMessage& msg) {
    auto str = msg.serialize();
    std::cout << "serialized raft message            : " << str << '\n';

    auto new_msg = RaftMessage::deserialize(str);
    std::cout << "deserialized then serialized again : " << new_msg.serialize()
              << '\n';

    assert(str == new_msg.serialize());
  };

  {
    RaftMessage msg;
    AppendEntriesRequestType append_req({0, 1, 100, 200, {}, 5});
    msg.loadAppendEntriesRequest(append_req);

    fun(msg);
  }
  {
    RaftMessage msg;
    AppendEntriesRequestType append_req({0, 1, 100, 200, {}, 5});
    append_req.entries.push_back({0, 1, 12345, 0, "test"});
    append_req.entries.push_back({1, 1, 12345, 0, "test1"});
    append_req.entries.push_back({2, 1, 12345, 0, "test2"});
    msg.loadAppendEntriesRequest(append_req);
    fun(msg);
  }

  {
    RaftMessage msg;
    AppendEntriesRequestType append_req({0, 1, 100, 200, {}, 5});
    append_req.entries.push_back({0, 1, 12345, 0, "naughty\0 sdfd"});
    append_req.entries.push_back({1, 1, 12345, 0, "test2 abc"});
    append_req.entries.push_back({2, 1, 12345, 1, ""});
    append_req.entries.push_back({3, 1, 12345, 0, "4th one!"});

    msg.loadAppendEntriesRequest(append_req);
    fun(msg);
  }

  {
    RaftMessage msg;
    msg.loadVoteRequest(RequestVoteRequestType{0, 1, 100, 200});
    fun(msg);
  }
  {
    RaftMessage msg;
    msg.loadVoteResult(RequestVoteResponseType{1, true});
    fun(msg);
  }
  {
    RaftMessage msg;
    msg.loadAppendEntriesResponse(AppendEntriesResponseType{1, 2, 1, 1});
    fun(msg);
  }
  {
    RaftMessage msg;
    msg.loadClientPutMessageRequest(
        ClientPutMessageType({"client put message"}));
    fun(msg);
  }

  {
    try {
      // error
      RaftMessage msg;
      fun(msg);
    } catch (const std::exception& e) {
      std::cout << e.what() << '\n';
    }
  }
}
