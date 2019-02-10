#pragma once 

#include <iostream>
#include <map>
#include <set>
#include <ctime>
#include <random>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <flowmq/message.hpp>
#include <flowmq/raft_message.hpp>
#include <flowmq/session.hpp>
#include <flowmq/cluster_manager.hpp>
#include <flowmq/client_manager.hpp>
#include <flowmq/message_queue.hpp>
#include <flowmq/log_entry_storage.hpp>
#include <flowmq/logging.hpp>
#include <flowmq/cluster_node_storage.hpp>
#include <flowmq/cluster_master.hpp>

using boost::asio::ip::tcp;

namespace flowmq{

// Forward declare
class ClusterMaster;

// This is the key player in the distributed message queue.
// ClusterNode implements Raft consensus algorithm for leader election 
// and log replication. 
//
// Internally uses ClusterManager to talk to 
// other nodes in the cluster. 
//
// Uses ClientManager 
// to manage connections with users of the message queue.
//
// Uses MessageQueue and LogEntryStorage to help 
// manage and persist the state of the message queue.

class ClusterMaster;

// POD object for general node configurations
struct ClusterNodeConfig{
    PartitionIdType partition_id;
    int node_id;
    int total_nodes; 
};

class ClusterNode{

    public:

        const int HEARTBEAT_EXPIRE_SECONDS = 3; // if no heartbeat received in these seconds, will start new vote
        const int HEARTBEAT_CHECK_INTERVAL = 5; // schedule interval to check heartbeats

        enum State {
            LEADER = 0, 
            FOLLOWER= 1, 
            CANDIDATE = 2 
        };

        // New constructor that takes a pointer to the cluster master and 
        // a pointer to the node storage object.
        //
        // Will start the node upon construction.
        ClusterNode(
                const ClusterNodeConfig& config,
                boost::asio::io_context& io_context, 
                ClusterMaster* cluster_master,
                std::unique_ptr<ClusterNodeStorageInterface> cluster_node_storage_p);

        ClusterNode() = delete;
        ClusterNode(const ClusterNode& ) = delete;
        ClusterNode& operator=(const ClusterNode&) = delete;
        ClusterNode(ClusterNode&& ) = delete;

        // handles all incoming messages from the cluster
        // this is run in the io_context thread.
        void message_handler(const Message& msg);

        // put all pending messages delivered to this consumer back to pending state
        void consumer_disconnected(int client_id);

    private:

        // handles deserialized message, this is run in the 
        // io_context of the current node_cluster instance.
        void local_message_handler(RaftMessage msg);

        void add_log_entry(const LogEntry entry);

        //this will send heart beats to all followers if current state is leader
        void start_send_hearbeat();

        //this will send vote request if current state is candidate
        void start_vote_scheduler();

        // statistics scheduler
        void start_statistics_scheduler();

        // TODO there is an issue here ... this is triggered in many places, 
        // including the heartbeat check response, and any response from appending a new log. 
        // Each error append entry response will trigger this, so there could be 
        // multiple series of req/resp to sync with a follower ... There should be only one, 
        // otherwise the process takes unnecessarily long time. 
        void trigger_entry_update(int follower_id);

        bool check_if_previous_log_in_sync(int prev_log_term, int prev_log_index);

        //will remove entries with index equal to or larger than index_to_remove
        void clean_log_entries_out_of_sync(int index_to_remove);

        // Takes a container of entries, the API of C should be similar to std::vector.
        // This works for protobuf::RepeatedPtrField<> as well.
        template<class C>
        void append_log_entries(int last_log_index, const C& new_entries);

        // Serialize RaftMessage into lower level Message before sending to transport layer
        // Will add the correct partition id as well.
        Message serialize_raft_message(RaftMessage& raft_message);

        // ---------------- Persisting state of the log entries

        // store log entry state to disk
        void store_log_entries_and_commit_index(int start_entry_index, int stop_entry_index);

        // ---------------- Message Queue Related Operations ----------------  
         
        // apply committed log entries to message queue
        void commit_log_entries(int start_entry_index, int stop_entry_index);
        // either put a new message or commit a delivered message
        void commit_log_entry(int entry_index);

        // fetch undelivered messages and send to consumers (if any exists)
        void trigger_message_delivery();

        // ---------------- Internal Variables -----------------------------------

        PartitionIdType partition_id_;
        int node_id_;
        int total_nodes_;
        boost::asio::io_context& io_context_;
        State state_;
        boost::asio::deadline_timer vote_timer_;
        boost::asio::deadline_timer heartbeat_timer_;
        boost::asio::deadline_timer stats_timer_;

        int cur_term_;
        int cur_leader_;
        time_t last_heart_beat_received_;
        int votes_collected_;
        int voted_for_; // -1 means did not vote yet

        //state for commits and index
        std::vector<LogEntry> log_entries_;
        int commit_index_;
        int last_applied_;
        std::map<int, int> next_index_; //map from node id to next index
        std::map<int, int> matched_index_; // map from node id to last matched index

        // persistence of log entries to disk
        std::unique_ptr<ClusterNodeStorageInterface> cluster_node_storage_p_;

        // in memory state for a message queue
        MessageQueue message_queue_;

        // Cluster Master Pointer (does not own it)
        ClusterMaster* cluster_master_;

};

// Takes a container of entries, the API of C should be similar to std::vector.
// This works for protobuf::RepeatedPtrField<> as well.
template<class C>
void ClusterNode::append_log_entries(int last_log_index, const C& new_entries){

    for(size_t i = 0; i < new_entries.size(); ++i){
        size_t index_in_log_entries = i + last_log_index + 1;

        if( index_in_log_entries > log_entries_.size() - 1){
            //new entry
            log_entries_.push_back(new_entries[i]); 
        }
        else if(log_entries_[index_in_log_entries].term() == new_entries[i].term()){
            //already exists and terms match
        }
        else {
            //does not match, need to cleanup
            clean_log_entries_out_of_sync(index_in_log_entries);
            log_entries_.push_back(new_entries[i]);
        }
    }
}



} // namespace flowmq


