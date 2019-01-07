
#pragma once

#include <set>
#include <map>
#include <vector>
#include <string>
#include <iostream>

// Class that helps to manage the state of a message queue. 
// There are two types of states: persisted state and temporary state.
// Persisted state is the result of applying a series of operations (log entries), 
// since log entries are persisted on disk, the persisted state can 
// be automatically recovered even if a node crashes. The persisted state
// is also replicated across all nodes. 
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

class MessageQueue{

    public:

        typedef int MessageId_t;
        typedef int ClientId_t;

        int insert_message(MessageId_t message_id, const std::string& message){

            if(message_store_.find(message_id) != message_store_.end()){
                //message already exists!
                return 1;
            }
            message_store_[message_id] = message;
            undelivered_messages_.insert(message_id);
            return 0;
        }

        int commit_message(MessageId_t message_id){
            if(message_store_.find(message_id) == message_store_.end()){
                //message does not exist!
                return 1;
            }

            if(message_id_to_consumer_.count(message_id) > 0){
                int consumer_id = message_id_to_consumer_[message_id];
                consumer_id_delivered_messages_[consumer_id].erase(message_id);
                message_id_to_consumer_.erase(message_id);
            }

            committed_messages_.insert(message_id);
            undelivered_messages_.erase(message_id); //this is necessary for followers as the trigger_message_delivery() is only run in the leader.
            std ::cout << "message " << message_id << " consumed!\n";

            return 0;
        }

        const std::string& get_message(MessageId_t message_id){
            return message_store_[message_id];
        }

        // these functions below manage the transient state
        int get_all_undelivered_messages(std::vector<MessageId_t>* message_ids){
            message_ids->insert(message_ids->end(), undelivered_messages_.begin(), undelivered_messages_.end());
            return 0;
        }

        // only leader will call this method
        int deliver_message_to_client_id(MessageId_t message_id, ClientId_t client_id){

            undelivered_messages_.erase(message_id);
            consumer_id_delivered_messages_[client_id].insert(message_id);
            message_id_to_consumer_[message_id] = client_id;
            std::cout << "Delivered message " << message_id << " to consumer " << client_id << '\n';

            return 0;
        }

        // client disconnected, put all delivered (but uncommitted) messages of this client back to the undelivered state
        int handle_client_disconnected(ClientId_t client_id){
            if(consumer_id_delivered_messages_.count(client_id) == 0) {
                std::cout << "ERROR consumer " << client_id << " not found (in consumer disconnected handler) \n";
                return 1;
            }

            for(auto& msg_id : consumer_id_delivered_messages_[client_id]){
                undelivered_messages_.insert(msg_id);
                message_id_to_consumer_.erase(msg_id);
            }
            consumer_id_delivered_messages_.erase(client_id);

            return 0;
        }

    private:

        // state for message queue
        std::map<MessageId_t, std::string> message_store_;  // message id to message

        // a message at anytime is only in one of the three states: undelivered(no consumers), delivered(but not committed yet), and committed.
        // delivered state is a temporary state, meaning it's not persisted on hard disk. When a cluster node crashes it reload 
        // all messages into message_store_, and committed message ids, all others are put into undelivered state.
        
        std::set<MessageId_t> undelivered_messages_;
        std::set<MessageId_t> committed_messages_;
        std::map<int, std::set<MessageId_t>> consumer_id_delivered_messages_; //consumer id to pending message ids
        std::map<MessageId_t, int> message_id_to_consumer_; //map from message id to consumer
 

};
