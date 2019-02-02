#include <flowmq/message_queue.hpp>
#include <flowmq/logging.hpp>

int MessageQueue::insert_message(MessageId_t message_id, const std::string& message){

    if(message_store_.find(message_id) != message_store_.end()){
        //message already exists!
        return 1;
    }
    message_store_[message_id] = message;
    undelivered_messages_.insert(message_id);
    return 0;
}

int MessageQueue::commit_message(MessageId_t message_id){
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

const std::string& MessageQueue::get_message(MessageId_t message_id){
    return message_store_[message_id];
}

// these functions below manage the transient state
int MessageQueue::get_all_undelivered_messages(std::vector<MessageId_t>* message_ids){
    message_ids->insert(message_ids->end(), undelivered_messages_.begin(), undelivered_messages_.end());
    return 0;
}

// only leader will call this method
int MessageQueue::deliver_message_to_client_id(MessageId_t message_id, ClientId_t client_id){

    undelivered_messages_.erase(message_id);
    consumer_id_delivered_messages_[client_id].insert(message_id);
    message_id_to_consumer_[message_id] = client_id;
    LOG_ERROR << "Delivered message " << message_id << " to consumer " << client_id << '\n';

    return 0;
}

// client disconnected, put all delivered (but uncommitted) messages of this client back to the undelivered state
int MessageQueue::handle_client_disconnected(ClientId_t client_id){
    if(consumer_id_delivered_messages_.count(client_id) == 0) {
        LOG_ERROR << "ERROR consumer " << client_id << " not found (in consumer disconnected handler) \n";
        return 1;
    }

    for(auto& msg_id : consumer_id_delivered_messages_[client_id]){
        undelivered_messages_.insert(msg_id);
        message_id_to_consumer_.erase(msg_id);
    }
    consumer_id_delivered_messages_.erase(client_id);

    return 0;
}


