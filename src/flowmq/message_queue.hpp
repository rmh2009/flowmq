#pragma once

#include <flowmq/basic_types.hpp>

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace flowmq {

// Class that helps to manage the state of a message queue.
// There are two types of states: persisted state and temporary state.
// Persisted state is the result of applying a series of operations (log
// entries), since log entries are persisted on disk, the persisted state can be
// automatically recovered even if a node crashes. The persisted state is also
// replicated across all nodes.
//
// Tempoerary state is only saved in memory, and usually only applies to the
// leader. This state includes transient information such as which message
// was delivered to which client id. This is required to support round-robin
// style delivery. If a service node crashes this state will be lost,
// the new leader will treat all uncommitted messages as undelivered and try
// to redeliver to other clients.
//
// This class is NOT thread safe, it is assumed that operations are serialized
// using some event loop, otherwise user of this class should manage the
// synchronization mechanism.
class MessageQueue {
 public:
  int insert_message(MessageIdType message_id, const std::string& message);
  int commit_message(MessageIdType message_id);

  const std::string& get_message(MessageIdType message_id);

  // these functions below manage the transient state
  int get_all_undelivered_messages(std::vector<MessageIdType>* message_ids);

  // only leader will call this method
  int deliver_message_to_client_id(MessageIdType message_id,
                                   ClientIdType client_id);

  // client disconnected, put all delivered (but uncommitted) messages of this
  // client back to the undelivered state
  int handle_client_disconnected(ClientIdType client_id);

 private:
  // state for message queue
  std::map<MessageIdType, std::string> message_store_;  // message id to message

  // a message at anytime is only in one of the three states: undelivered(no
  // consumers), delivered(but not committed yet), and committed. delivered
  // state is a temporary state, meaning it's not persisted on hard disk. When a
  // cluster node crashes it reload all messages into message_store_, and
  // committed message ids, all others are put into undelivered state.

  std::set<MessageIdType> undelivered_messages_;
  std::set<MessageIdType> committed_messages_;
  std::map<ClientIdType, std::set<MessageIdType>>
      consumer_id_delivered_messages_;  // consumer id to pending message ids
  std::map<MessageIdType, ClientIdType>
      message_id_to_consumer_;  // map from message id to consumer
};

}  // namespace flowmq

