#pragma once 

#include <iostream>
#include <map>
#include <set>
#include <ctime>
#include <random>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/raft_message.hpp>
#include <mymq/session.hpp>
#include <mymq/cluster_manager.hpp>
#include <mymq/client_manager.hpp>
#include <mymq/message_queue.hpp>
#include <mymq/log_entry_storage.hpp>

using boost::asio::ip::tcp;

namespace flowmq{

// Player in a distributed system using 
// Raft consensus algorithm for leader election 
// and log replication. 
//
// Internally uses ClusterManager to talk to 
// other nodes in the cluster.
//
// Uses other in-memory and on-disk 
// log manangement classes.

namespace {
const char* LOG_ENTRY_STORAGE_PREFIX = "storage_log_entry_";
const char* METADATA_STORAGE_PREFIX  = "storage_metadata_";
}

class ClusterNode{

    public:

        const int HEARTBEAT_EXPIRE_SECONDS = 3; // if no heartbeat received in these seconds, will start new vote
        const int HEARTBEAT_CHECK_INTERVAL = 5; // schedule interval to check heartbeats

        enum State {
            LEADER = 0, 
            FOLLOWER= 1, 
            CANDIDATE = 2 
        };

        template<class... ARGS>
            ClusterNode(const tcp::endpoint& client_facing_endpoint, 
                    int node_id, 
                    int total_nodes, 
                    boost::asio::io_context& io_context, 
                    ARGS&&... args):
                node_id_(node_id),
                total_nodes_(total_nodes),
                io_context_(io_context),
                cluster_manager_(io_context, std::forward<ARGS>(args)...),
                state_(CANDIDATE),
                vote_timer_(io_context),
                heartbeat_timer_(io_context),
                stats_timer_(io_context),
                cur_term_(0),
                voted_for_(-1),
                commit_index_(0),
                last_applied_(0),
                client_manager_(io_context, client_facing_endpoint)

        {
            //initialize random seed
            
            std::cout << "setting random seed according to pid " << getpid() << '\n';
            std::srand(getpid());

            //initialize state
            for(int i = 0; i < total_nodes; ++i){
                if(i == node_id) continue;
                next_index_[i] = 1;
                matched_index_[i] = 0;
            }

            flowmq::LogEntry log_entry;
            log_entry.set_term(0);
            log_entry.set_term(0);
            log_entry.set_term(0);
            LogEntry entry;
            log_entries_.push_back(std::move(entry));
            //log_entries_.push_back(LogEntry{0, 0, 0, 0, ""});

            std::cout << "init: " << log_entries_.size() << ", " << log_entries_[0].term() << '\n';
            check_if_previous_log_in_sync(0, 0);

            //scheduler to check hearbeat and initiate vote periodically
            start_vote_scheduler();
            start_send_hearbeat();
            start_statistics_scheduler();

            // load persisted log entries
            storage_log_entry_filename_ = LOG_ENTRY_STORAGE_PREFIX + std::to_string(node_id) + ".data";
            storage_metadata_file_name_ = METADATA_STORAGE_PREFIX + std::to_string(node_id) + ".data";
            if(0 != LogEntryStorage::load_log_entry_from_file(storage_log_entry_filename_, &log_entries_)){
                std::cout << "WARNING : Error loading log entries from storage\n";
            }
            for(size_t i = 1; i < log_entries_.size(); ++i){
                commit_log_entry(i); //update the queue state, this should not trigger delivery to client
            }
            LogEntryMetaData metadata{0};
            if(0 == MetadataStorage::load_metadata_from_file(storage_metadata_file_name_, &metadata)){
                commit_index_ = metadata.last_committed;
            }
            std::cout << "after loading data: commit index is : " << commit_index_ << ", log entry size: " << log_entries_.size() << '\n';

            // start cluster
            cluster_manager_.register_handler(std::bind(&ClusterNode::message_handler, this, std::placeholders::_1));
            cluster_manager_.start();

            // start client manager
            client_manager_.register_handler(std::bind(&ClusterNode::message_handler, this, std::placeholders::_1));
            client_manager_.register_disconnect_handler(std::bind(&ClusterNode::consumer_disconnected, this, std::placeholders::_1));
            client_manager_.start();

        }

        ClusterNode() = delete;
        ClusterNode(const ClusterNode& ) = delete;
        ClusterNode& operator=(const ClusterNode&) = delete;
        ClusterNode(ClusterNode&& ) = delete;

    private:
        void add_log_entry(const LogEntry entry){
            // this should always be run in io_context, client should not directly call this
            
            log_entries_.push_back(entry);
            for(auto& endpoint : next_index_){ //get endpoints from next_index_ map
                trigger_entry_update(endpoint.first);
            }

        }


        //this will send heart beats to all followers if current state is leader
        void start_send_hearbeat(){
            
            std::cout << "sending heartbeat (if leader) ... ... \n";

            heartbeat_timer_.expires_from_now(boost::posix_time::seconds(HEARTBEAT_EXPIRE_SECONDS));
            heartbeat_timer_.async_wait([this](boost::system::error_code ){

                    if(state_ == LEADER){
                    std::cout << " *************** node " << node_id_ << " : I'm the leader, sending heartbeat now! *************\n";
                    RaftMessage msg;
                    int last_log_term = log_entries_.back().term();
                    int last_log_index = log_entries_.back().index();
                    AppendEntriesRequestType req;
                    req.set_term(cur_term_);
                    req.set_leader_id(node_id_);
                    req.set_prev_log_index(last_log_index);
                    req.set_prev_log_term(last_log_term);
                    req.set_leader_commit(commit_index_);
                    msg.loadAppendEntriesRequest(std::move(req));
                    
                    //msg.loadAppendEntriesRequest(AppendEntriesRequestType(
                    //            {cur_term_, node_id_, last_log_index, last_log_term, {}, commit_index_}));

                    cluster_manager_.broad_cast(serialize_raft_message(msg));
                    }

                    start_send_hearbeat();

                    });
        }

        //this will send vote request if current state is candidate
        void start_vote_scheduler(){

            std::cout << "checking hearbeat ... \n";
            //check repeatedly
            vote_timer_.expires_from_now(boost::posix_time::seconds(HEARTBEAT_CHECK_INTERVAL));
            vote_timer_.async_wait([this](boost::system::error_code ){

                    time_t now;
                    std::time(&now);

                    // reset state to CANDIDATE if haven't received heart beat for some time.
                    if(state_ == FOLLOWER && last_heart_beat_received_ + HEARTBEAT_EXPIRE_SECONDS + 2 < now){ //2 is the margin in case there was a delay in receiving heartbeats
                    state_ = CANDIDATE;
                    }

                    //starting new election round if still in candidate state
                    if(state_ == CANDIDATE){
                    ++cur_term_;
                    //reset voting related states
                    voted_for_ = -1;
                    votes_collected_ = 0; 

                    RaftMessage raft_msg;
                    int last_log_term = log_entries_.back().term();
                    int last_log_index = log_entries_.back().index();
                    RequestVoteRequestType req;
                    req.set_term(cur_term_);
                    req.set_candidate_id(node_id_);
                    req.set_last_log_index(last_log_index);
                    req.set_last_log_term(last_log_term);
                    raft_msg.loadVoteRequest(std::move(req));

                    //raft_msg.loadVoteRequest(RequestVoteRequestType({cur_term_, node_id_, last_log_index, last_log_term}));

                    //set a random timer to send vote request so that all candidates will not send this request at the same time.
                    auto timer = std::make_shared<boost::asio::deadline_timer>(io_context_);
                    int delay = std::rand() % 1000 ;
                    std::cout << "random delay in start_vote_scheduler : " << delay << " miliseconds \n";
                    timer -> expires_from_now(boost::posix_time::milliseconds(delay));
                    timer->async_wait([timer, this, raft_msg](boost::system::error_code const&){
                            if(voted_for_ != -1) return; //already voted
                            voted_for_ = node_id_;
                            votes_collected_++; //vote for self
                            cluster_manager_.broad_cast(serialize_raft_message(raft_msg));
                            });
                    }

                    start_vote_scheduler();
            });

        }

        // statistics scheduler
        void start_statistics_scheduler(){
            stats_timer_.expires_from_now(boost::posix_time::seconds(5));
            stats_timer_.async_wait([this](boost::system::error_code ){

                    std::cout << "Printing statistics \n ___________________________________________________________________________\n\n";

                    for(auto entry : log_entries_){
                    std::cout << entry.index() << ',' << entry.operation() << ',' << entry.message() << "   ";
                    }

                    std::cout << "\ncommit_index : " << commit_index_ << '\n';

                    std::cout << "\nundelivered_messages : \n";
                    std::vector<MessageQueue::MessageId_t> ids;
                    message_queue_.get_all_undelivered_messages(&ids);
                    for(auto id : ids) {
                    std::cout << id << ", ";
                    }

                    //std::cout << "\ncommitted messages : \n";
                    //for(auto id :committed_messages_){
                    //std::cout << id << ", ";
                    //}

                    std::cout << "\n ___________________________________________________________________________\n";
                    start_statistics_scheduler();

                    });

        }

        // handles all incoming messages from the cluster
        // this is run in the io_context thread.
        void message_handler(const Message& msg){

            RaftMessage raft_msg = RaftMessage::deserialize_from_message(msg);
            std::cout << "#] " << raft_msg.DebugString() << '\n';

            switch(raft_msg.type()){
                case RaftMessage::APPEND_ENTRIES_REQUEST: 
                    {
                        const AppendEntriesRequestType& req = raft_msg.get_append_request();
                        //check if term is up-to-date, if not send back current term, leader should update its term.
                        if(req.term() < cur_term_){
                            std::cout << "WARNING: Received expired heart beat from leader! sending back current term \n";

                            RaftMessage msg;
                            AppendEntriesResponseType resp;
                            resp.set_term(cur_term_);
                            resp.set_follower_id(node_id_);
                            resp.set_append_result_success(0); //0 is failure
                            resp.set_last_index_synced(-1);
                            msg.loadAppendEntriesResponse(std::move(resp));
                            
                            //msg.loadAppendEntriesResponse(AppendEntriesResponseType({cur_term_, node_id_, 0, -1}));
                            cluster_manager_.write_message(req.leader_id(), serialize_raft_message(msg));
                            return;
                        }

                        //convert to follower
                        if( state_ == CANDIDATE || (state_ == LEADER && req.term() > cur_term_)){
                            std::cout << "Received new leader id " << req.leader_id() << "\n";
                            state_ = FOLLOWER;
                        }

                        //update last_heart_beat_received_
                        std::time(&last_heart_beat_received_);
                        cur_term_ = req.term();

                       
                        //check if previous log in sync, if not return false
                        if(!check_if_previous_log_in_sync(req.prev_log_term(), req.prev_log_index())){
                            std::cout << "ERROR: previous log not in sync! " << req.DebugString() << '\n';
                            RaftMessage msg;
                            AppendEntriesResponseType resp;
                            resp.set_term(cur_term_);
                            resp.set_follower_id(node_id_);
                            resp.set_append_result_success(0); //0 is failure
                            resp.set_last_index_synced(-1);
                            msg.loadAppendEntriesResponse(std::move(resp));
 
                            //msg.loadAppendEntriesResponse(AppendEntriesResponseType({cur_term_, node_id_, 0, -1}));
                            cluster_manager_.write_message(req.leader_id(), serialize_raft_message(msg));
                            return;
                        }

                        //update leader commit
                        if(commit_index_ < req.leader_commit() && req.leader_commit() < (int)log_entries_.size()){
                            commit_log_entries(commit_index_ + 1, req.leader_commit() + 1);
                            store_log_entries_and_commit_index(commit_index_ + 1, req.leader_commit() + 1);
                            commit_index_ = req.leader_commit();
                        }

                        //append logs from leader
                        append_log_entries(req.prev_log_index(), req.entries());

                        //we've updated our logs, return success!
                        RaftMessage msg;
                        AppendEntriesResponseType resp;
                        resp.set_term(cur_term_);
                        resp.set_follower_id(node_id_);
                        resp.set_append_result_success(1); // 1 is success!
                        resp.set_last_index_synced(log_entries_.back().index());
                        msg.loadAppendEntriesResponse(std::move(resp));
 
                        //msg.loadAppendEntriesResponse(AppendEntriesResponseType({cur_term_, node_id_, 1, log_entries_.back().index}));
                        cluster_manager_.write_message(req.leader_id(), serialize_raft_message(msg));
                        return;

                        break;
                    }
                case RaftMessage::APPEND_ENTRIES_RESPONSE: 
                    {
                        const AppendEntriesResponseType& resp = raft_msg.get_append_response();

                        int follower_id = resp.follower_id();
                        int last_index_synced = resp.last_index_synced();

                        if(resp.append_result_success()){
                            //received success!
                            // This is very tricky, matched_index_ should be increasing only, however in the scenario where one 
                            // node crashed and lost all of its data, then restarted, the matched_index_[node_id] for that node 
                            // should be allowed to rollback to a smaller value ... however it seems allowing this would break 
                            // the assumptions required to guarantee the consensus of the Raft algorithm, because leader could no longer 
                            // make assumptions that majority nodes have committed an entry if any node could lose committed data ...
                            
                            // version 1, matched index increases only, this causes some slowness issues when a node could lose persisted data and recover as a clean node
                            //matched_index_[follower_id] = std::max(matched_index_[follower_id], last_index_synced);
                            // version 2, matched index is allowed to decrease, I prefer this one for now but needs more testing and proof
                            matched_index_[follower_id] =  last_index_synced;
                            
                            if(commit_index_ < resp.last_index_synced()){
                                //update commit_index_
                                int count_synced_nodes = 0;
                                for(auto index : matched_index_){
                                    if(index.second >= resp.last_index_synced()) count_synced_nodes++;
                                }
                                if(count_synced_nodes + 1 > total_nodes_ / 2){
                                    std::cout << "index " << resp.last_index_synced() << " is synced among majority nodes, will commit this index.\n";
                                    commit_log_entries(commit_index_ + 1, resp.last_index_synced() + 1); // left close right open 
                                    store_log_entries_and_commit_index(commit_index_ + 1, resp.last_index_synced() + 1);
                                    commit_index_ = std::max(commit_index_, resp.last_index_synced());
                                }
                                else{
                                    std::cout << "index " << resp.last_index_synced() << " is not synced among majority nodes yet, will not commit this index.\n";
                                }
                            }

                            //set next index to update to be after the matched index
                            next_index_[follower_id] = matched_index_[follower_id]+1; 
                        }
                        else if (resp.term() > cur_term_){
                            //failed due to expired term
                            std::cout << "Updating current term to " << resp.term() << '\n';
                            cur_term_ = resp.term();
                        }
                        else{
                            //failed due to out of sync, try send earlier entries
                            next_index_[follower_id] = std::max(1, next_index_[follower_id] - 1);
                        }
                        
                        trigger_entry_update(follower_id);


                        break;
                    }
                case RaftMessage::REQUEST_VOTE_REQUEST:
                    {
                        const RequestVoteRequestType& req = raft_msg.get_vote_request();

                        int candidate_node_id = req.candidate_id();
                        int request_term = req.term();

                        // TODO, must handle this case:  if req.last_log_index and req.last_log_term is older than current log entries, reject this vote
                        if(req.term() < cur_term_ || 
                                (req.term() == cur_term_ && voted_for_ != -1 && voted_for_ != req.candidate_id())){ //we voted for somebody else this term!
                            if(voted_for_ != -1){
                                std::cout << "ERROR: Already voted " << voted_for_ << " this term!\n";
                            }
                            else{
                                std::cout << "ERROR: Received request vote request from node that has a lower term than current! Ignoring this request!\n";
                            }

                            RaftMessage msg;
                            RequestVoteResponseType resp;
                            resp.set_term(request_term);
                            resp.set_vote_result_term_granted(0); // 0 is not granted
                            msg.loadVoteResult(std::move(resp));

                            cluster_manager_.write_message(candidate_node_id, serialize_raft_message(msg));
                            return;
                        }

                        if(req.term() > cur_term_){
                            cur_term_ = request_term;
                            votes_collected_ = 0;
                        }

                        //grant vote request
                        if(state_ == CANDIDATE || state_ == FOLLOWER){
                            RaftMessage msg;
                            voted_for_ = candidate_node_id;
                            std::cout << "voting for " << candidate_node_id << '\n';
                            RequestVoteResponseType resp;
                            resp.set_term(request_term);
                            resp.set_vote_result_term_granted(1); // 1 is granted
                            msg.loadVoteResult(std::move(resp));

                            //msg.loadVoteResult(RequestVoteResponseType({req.term, 1}));
                            cluster_manager_.write_message(candidate_node_id, serialize_raft_message(msg));
                        }

                        break;
                    }
                case RaftMessage::REQUEST_VOTE_RESPONSE:
                    {
                        if(state_ != CANDIDATE){
                            std::cout << "ERROR: received request vote response, but current state is not candidate! current state: " << state_ << "\n";
                            return;
                        }

                        const RequestVoteResponseType& resp = raft_msg.get_vote_response();
                        if(resp.term() < cur_term_){
                            std::cout << "ERROR: Received request vote response with a term lower than current! Ignoring this response!\n";
                            return;
                        }

                        if(resp.vote_result_term_granted() == 1)
                            votes_collected_++;
                        if(votes_collected_ > total_nodes_ / 2){
                            // Voted as the new LEADER!!
                            std::cout << "node " << node_id_ << " : I am the new leader!\n";
                            state_ = LEADER;
                            cur_term_ = resp.term();
                        }

                        break;
                    }
                case RaftMessage::CLIENT_PUT_MESSAGE:
                    {
                        if(state_ != LEADER){
                            std::cout << "ERROR! current node is not leader! only leader accepts put message reqeust.\n";
                            return;
                        }
                        const ClientPutMessageType& req = raft_msg.get_put_message_request();

                        int random_id = std::rand();
                        LogEntry entry;
                        entry.set_index((int)log_entries_.size());
                        entry.set_term(cur_term_);
                        entry.set_message_id(random_id);
                        entry.set_operation(LogEntry::ADD);
                        entry.set_message(req.message());
                        //LogEntry entry({(int)log_entries_.size(), cur_term_, random_id, LogEntry::ADD, req.message});
                        add_log_entry(std::move(entry));
                        break;
                    }
                case RaftMessage::CLIENT_OPEN_QUEUE:
                    {
                        if(state_ != LEADER){
                            std::cout << "ERROR! current node is not leader! only leader accepts open queue reqeust.\n";
                            return;
                        }

                        trigger_message_delivery();
                        break;
                    }
                case RaftMessage::CLIENT_COMMIT_MESSAGE:
                    {
                        if(state_ != LEADER){
                            std::cout << "ERROR! current node is not leader! only leader accepts commit message reqeust.\n";
                            return;
                        }

                        const ClientCommitMessageType& req = raft_msg.get_commit_message_request();
                        LogEntry entry;
                        entry.set_index((int)log_entries_.size());
                        entry.set_term(cur_term_);
                        entry.set_message_id(req.message_id());
                        entry.set_operation(LogEntry::COMMIT);
                        entry.set_message("");

                        //LogEntry entry({(int)log_entries_.size(), cur_term_, req.message_id, LogEntry::COMMIT, ""});
                        add_log_entry(std::move(entry));
                        break;
                    }
                default :
                    break;
            }
        }

        // TODO there is an issue here ... this is triggered in many places, 
        // including the heartbeat check response, and any response from appending a new log. 
        // Each error append entry response will trigger this, so there could be 
        // multiple series of req/resp to sync with a follower ... There should be only one, 
        // otherwise the process takes unnecessarily long time. 
        void trigger_entry_update(int follower_id){

            int index_to_send = next_index_[follower_id];
            //assume 0 is always synced, so next index should be >= 1
            if(index_to_send >=1 && index_to_send <= (int)log_entries_.size() - 1){

                int prev_index = next_index_[follower_id] - 1;
                int prev_term = log_entries_[prev_index].term();

                RaftMessage msg;
                AppendEntriesRequestType req;
                req.set_term(cur_term_);
                req.set_leader_id(node_id_);
                req.set_prev_log_index(prev_index);
                req.set_prev_log_term(prev_term);
                auto new_elem = req.mutable_entries()->Add();
                (*new_elem) = log_entries_[index_to_send];
                req.set_leader_commit(commit_index_);
                msg.loadAppendEntriesRequest(std::move(req));

                //msg.loadAppendEntriesRequest(AppendEntriesRequestType{
                //        cur_term_, node_id_, prev_index, prev_term, std::vector<LogEntry>{log_entries_[index_to_send]},
                //        commit_index_
                //        });

                cluster_manager_.write_message(follower_id, serialize_raft_message(msg));

                //to slow things down and help debug
                //usleep(1000000);

            }

        }

        bool check_if_previous_log_in_sync(int prev_log_term, int prev_log_index){
            if((int)log_entries_.size() < prev_log_index + 1) {
                return false;
            }
            if(log_entries_[prev_log_index].term() != prev_log_term){
                return false;
            }
            return true;
        }

        //will remove entries with index equal to or larger than index_to_remove
        void clean_log_entries_out_of_sync(int index_to_remove){
            log_entries_.erase(log_entries_.begin() + index_to_remove, log_entries_.end());
        }

        // Takes a container of entries, the API of C should be similar to std::vector.
        // This works for protobuf::RepeatedPtrField<> as well.
        template<class C>
        void append_log_entries(int last_log_index, const C& new_entries){

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

        Message serialize_raft_message(const RaftMessage& raft_message){
            Message msg;
            raft_message.serialize_to_message(&msg);
            return msg;
        }


        // ---------------- Message Queue Related Operations ----------------  
         
        // storage related
        void store_log_entries_and_commit_index(int start_entry_index, int stop_entry_index){
            assert(start_entry_index < stop_entry_index);

            std::vector<LogEntry> temp_entries(log_entries_.begin() + start_entry_index, log_entries_.begin() + stop_entry_index);
            LogEntryStorage::append_log_entry_to_file(storage_log_entry_filename_, temp_entries);
            LogEntryMetaData metadata({stop_entry_index - 1});
            MetadataStorage::save_metadata_to_file(storage_metadata_file_name_, metadata);
        }
        // commit log entries to message queue
        void commit_log_entries(int start_entry_index, int stop_entry_index){
            while(start_entry_index < stop_entry_index){
                commit_log_entry(start_entry_index);
                ++start_entry_index;
            }
            trigger_message_delivery();
        }
        // either put a new message or commit a delivered message
        void commit_log_entry(int entry_index){
            if(entry_index <= 0) return;
            if(entry_index >= (int)log_entries_.size()){
                std::cout << "ERROR: commit index " << entry_index << " is out of bound!\n";
                return;
            }

            //committing now
            if(log_entries_[entry_index].operation() == LogEntry::ADD){ //add
                message_queue_.insert_message(log_entries_[entry_index].message_id(), log_entries_[entry_index].message());
            }
            else if(log_entries_[entry_index].operation() == LogEntry::COMMIT){
                int message_id = log_entries_[entry_index].message_id();
                message_queue_.commit_message(message_id);
            }
            
        }

        // put all pending messages delivered to this consumer back to pending state
        void consumer_disconnected(int client_id){
            message_queue_.handle_client_disconnected(client_id);
            trigger_message_delivery();
        }

        // fetch undelivered messages and send to consumers (if any exists)
        void trigger_message_delivery(){

            std::cout << "trying to deliver messages \n";
            if(!client_manager_.has_consumers()) return;
            std::vector<MessageQueue::MessageId_t> id_temp;
            message_queue_.get_all_undelivered_messages(&id_temp);

            for(auto message_id : id_temp){
                RaftMessage msg;
                ServerSendMessageType send;
                send.set_message_id(message_id);
                send.set_message(message_queue_.get_message(message_id));
                msg.loadServerSendMessageRequest(std::move(send));
                //msg.loadServerSendMessageRequest(ServerSendMessageType{message_id, message_queue_.get_message(message_id)});
                std::cout << "delivering message to client : " << msg.DebugString() << '\n';
                int consumer_id = client_manager_.deliver_one_message_round_robin(serialize_raft_message(msg));
                if(consumer_id == -1){
                    std::cout << "ERROR: unable to deliver message " << message_id << " to consumer!\n";
                    return;
                }
                message_queue_.deliver_message_to_client_id(message_id, consumer_id);
            }
        }

        int node_id_;
        int total_nodes_;
        boost::asio::io_context& io_context_;
        ClusterManager cluster_manager_;
        State state_;
        boost::asio::deadline_timer vote_timer_;
        boost::asio::deadline_timer heartbeat_timer_;
        boost::asio::deadline_timer stats_timer_;

        int cur_term_;
        time_t last_heart_beat_received_;
        int votes_collected_;
        int voted_for_; // -1 means did not vote yet

        // storge files
        std::string storage_log_entry_filename_;
        std::string storage_metadata_file_name_;

        //state for commits and index
        std::vector<LogEntry> log_entries_;
        int commit_index_;
        int last_applied_;
        std::map<int, int> next_index_; //map from node id to next index
        std::map<int, int> matched_index_; // map from node id to last matched index

        // state for a message queue
        MessageQueue message_queue_;

        // state for publisher/consumers
        ClientManager client_manager_;

};

} // namespace flowmq


