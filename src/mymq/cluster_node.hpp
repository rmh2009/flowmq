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

using boost::asio::ip::tcp;

// Player in a distributed system using 
// Raft consensus algorithm for leader election 
// and log replication. 
//
// Internally uses ClusterManager to talk to 
// other nodes in the cluster.
//
// Uses other in-memory and on-disk 
// log manangement classes.
//

struct LogEntry{

    int index;
    int term;
    std::string msg;
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

        template<class... ARGS>
            ClusterNode(int node_id, int total_nodes, boost::asio::io_context& io_context, ARGS&&... args):
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
                last_applied_(0)
        {
            //initialize random seed
            
            auto ut = boost::posix_time::microsec_clock::universal_time();
            auto duration = ut.time_of_day();
            std::cout << "setting random seed according to microseconds" << duration.total_microseconds() % 1000000
                << " and pid " << getpid() << '\n';
            std::srand(getpid() + duration.total_microseconds() % 1000000);

            //initialize state
            for(int i = 0; i < total_nodes; ++i){
                if(i == node_id) continue;
                next_index_[i] = 1;
                matched_index_[i] = 0;
            }

            log_entries_.push_back(LogEntry{0, 0, ""});

            std::cout << "init: " << log_entries_.size() << ", " << log_entries_[0].term << '\n';
            check_if_previous_log_in_sync(0, 0);

            //scheduler to check hearbeat and initiate vote periodically
            start_vote_scheduler();
            start_send_hearbeat();
            start_statistics_scheduler();
            cluster_manager_.register_handler(std::bind(&ClusterNode::message_handler, this, std::placeholders::_1));
            cluster_manager_.start();
        }

        ClusterNode() = delete;
        ClusterNode(const ClusterNode& ) = delete;
        ClusterNode& operator=(const ClusterNode&) = delete;
        ClusterNode(ClusterNode&& ) = delete;

    private:
        void add_log_entry(const std::string& msg){
            // this should always be run in io_context, client should not directly call this

            log_entries_.push_back(LogEntry({(int)log_entries_.size(), cur_term_, msg}));
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
                    int last_log_term = log_entries_.back().term;
                    int last_log_index = log_entries_.back().index;
                    msg.loadAppendEntriesRequest(AppendEntriesRequestType(
                                {cur_term_, node_id_, last_log_index, last_log_term, std::vector<std::string>(), std::vector<int>(), last_log_index}));

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
                    votes_collected_ = 1; //always vote self first

                    RaftMessage raft_msg;
                    int last_log_term = log_entries_.back().term;
                    int last_log_index = log_entries_.back().index;
                    raft_msg.loadVoteRequest(RequestVoteRequestType({cur_term_, node_id_, last_log_index, last_log_term}));

                    //set a random timer to send vote request so that all candidates will not send this request at the same time.
                    auto timer = std::make_shared<boost::asio::deadline_timer>(io_context_);
                    int delay = 1000 + std::rand() % 1000 ;
                    std::cout << "random delay in start_vote_scheduler : " << delay << " miliseconds \n";
                    timer -> expires_from_now(boost::posix_time::milliseconds(delay));
                    timer->async_wait([timer, this, raft_msg](boost::system::error_code const&){
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
                    std::cout << "{" << entry.index << " , " << entry.term << " , " << entry.msg << "}, ";
                    }

                    std::cout << "commit_index : " << commit_index_ << '\n';

                    std::cout << "\n ___________________________________________________________________________\n";
                    start_statistics_scheduler();

                    });

        }

        // handles all incoming messages from the cluster
        // this is run in the io_context thread.
        void message_handler(const Message& msg){

            RaftMessage raft_msg = RaftMessage::deserialize(std::string(msg.body(), msg.body_length()));

            switch(raft_msg.type()){
                case RaftMessage::AppendEntriesRequest : 
                    {
                        const AppendEntriesRequestType& req = raft_msg.get_append_request();
                        //check if term is up-to-date, if not send back current term, leader should update its term.
                        if(req.term < cur_term_){
                            std::cout << "WARNING: Received expired heart beat from leader! sending back current term \n";

                            RaftMessage msg;
                            msg.loadAppendEntriesResponse(AppendEntriesResponseType({cur_term_, node_id_, 0, -1}));
                            cluster_manager_.write_message(req.leader_id, serialize_raft_message(msg));
                            return;
                        }

                        //convert to follower
                        if( state_ == CANDIDATE || (state_ == LEADER && req.term > cur_term_)){
                            std::cout << "Received new leader id " << req.leader_id << "\n";
                            state_ = FOLLOWER;
                        }

                        //update last_heart_beat_received_
                        std::time(&last_heart_beat_received_);
                        cur_term_ = req.term;

                        //update leader commit
                        commit_index_ = req.leader_commit;

                        //check if previous log in sync, if not return false
                        if(!check_if_previous_log_in_sync(req.prev_log_term, req.prev_log_index)){
                            std::cout << "ERROR: previous log not in sync! " << req.prev_log_term << ", " << req.prev_log_index << '\n';
                            RaftMessage msg;
                            msg.loadAppendEntriesResponse(AppendEntriesResponseType({cur_term_, node_id_, 0, -1}));
                            cluster_manager_.write_message(req.leader_id, serialize_raft_message(msg));
                            return;
                        }

                        //append logs from leader
                        append_log_entries(req.prev_log_index, req.entries, req.entry_terms);

                        //we've updated our logs, return success!
                        RaftMessage msg;
                        msg.loadAppendEntriesResponse(AppendEntriesResponseType({cur_term_, node_id_, 1, (int)log_entries_.size() - 1}));
                        cluster_manager_.write_message(req.leader_id, serialize_raft_message(msg));
                        return;

                        break;
                    }
                case RaftMessage::AppendEntriesResponse : 
                    {
                        const AppendEntriesResponseType& resp = raft_msg.get_append_response();

                        int follower_id = resp.follower_id;
                        int last_index_synced = resp.last_index_synced;

                        if(resp.append_result_success){
                            //received success!
                            matched_index_[follower_id] = std::max(matched_index_[follower_id], last_index_synced);
                            
                            if(commit_index_ < resp.last_index_synced){
                                //update commit_index_
                                int count_synced_nodes = 0;
                                for(auto index : matched_index_){
                                    if(index.second >= resp.last_index_synced) count_synced_nodes++;
                                }
                                if(count_synced_nodes > total_nodes_ / 2){
                                    std::cout << "index " << resp.last_index_synced << " is synced among majority nodes, will commit this index.\n";
                                    commit_index_ = std::max(commit_index_, resp.last_index_synced);
                                }
                                else{
                                    std::cout << "index " << resp.last_index_synced << " is not synced among majority nodes yet, will not commit this index.\n";
                                }
                            }

                            //set next index to update to be after the matched index
                            next_index_[follower_id] = matched_index_[follower_id]+1; 
                        }
                        else if (resp.term > cur_term_){
                            //failed due to expired term
                            std::cout << "Updating current term to " << resp.term << '\n';
                            cur_term_ = resp.term;
                        }
                        else{
                            //failed due to out of sync, try send earlier entries
                            next_index_[follower_id] = std::max(1, next_index_[follower_id] - 1);
                        }
                        
                        trigger_entry_update(follower_id);


                        break;
                    }
                case RaftMessage::RequestVoteRequest:
                    {
                        const RequestVoteRequestType& req = raft_msg.get_vote_request();

                        int candidate_node_id = req.candidate_id;
                        int request_term = req.term;

                        // TODO, must handle this case:  if req.last_log_index and req.last_log_term is older than current log entries, reject this vote
                        if(req.term < cur_term_ || 
                                (voted_for_ != -1 && voted_for_ != req.candidate_id)){ //we voted for somebody else this term!
                            if(voted_for_ != -1){
                                std::cout << "ERROR: Already voted " << voted_for_ << " this term!\n";
                            }
                            else{
                                std::cout << "ERROR: Received request vote request from node that has a lower term than current! Ignoring this request!\n";
                            }

                            RaftMessage msg;
                            msg.loadVoteResult(RequestVoteResponseType({request_term, 0})); // 0 is not granted
                            cluster_manager_.write_message(candidate_node_id, serialize_raft_message(msg));
                            return;
                        }

                        //grant vote request
                        if(state_ == CANDIDATE || state_ == FOLLOWER){
                            RaftMessage msg;
                            voted_for_ = candidate_node_id;
                            std::cout << "voting for " << candidate_node_id << '\n';
                            msg.loadVoteResult(RequestVoteResponseType({req.term, 1}));
                            cluster_manager_.write_message(candidate_node_id, serialize_raft_message(msg));
                        }

                        break;
                    }
                case RaftMessage::RequestVoteResponse:
                    {
                        if(state_ != CANDIDATE){
                            std::cout << "ERROR: received request vote response, but current state is not candidate! current state: " << state_ << "\n";
                            return;
                        }

                        const RequestVoteResponseType& resp = raft_msg.get_vote_response();
                        if(resp.term < cur_term_){
                            std::cout << "ERROR: Received request vote response with a term lower than current! Ignoring this response!\n";
                            return;
                        }

                        if(resp.vote_result_term_granted == 1)
                            votes_collected_++;
                        if(votes_collected_ > total_nodes_ / 2){
                            // Voted as the new LEADER!!
                            std::cout << "node " << node_id_ << " : I am the new leader!\n";
                            state_ = LEADER;
                            cur_term_ = resp.term;
                        }

                        break;
                    }
                case RaftMessage::ClientPutMessage:
                    {
                        if(state_ != LEADER){
                            std::cout << "ERROR! current node is not leader! only leader accepts put message reqeust.\n";
                            return;
                        }
                        const ClientPutMessageType& req = raft_msg.get_put_message_request();
                        add_log_entry(req.message);
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
                int prev_term = log_entries_[prev_index].term;

                RaftMessage msg;
                msg.loadAppendEntriesRequest(AppendEntriesRequestType{
                        cur_term_, node_id_, prev_index, prev_term, std::vector<std::string>{log_entries_[index_to_send].msg},
                        std::vector<int>{log_entries_[index_to_send].term},
                        commit_index_
                        });

                cluster_manager_.write_message(follower_id, serialize_raft_message(msg));

                //to slow things down and help debug
                //usleep(1000000);

            }

        }

        bool check_if_previous_log_in_sync(int prev_log_term, int prev_log_index){
            std::cout << log_entries_.size() << ", " << log_entries_[prev_log_index].term << '\n';
            if((int)log_entries_.size() < prev_log_index + 1) {
                return false;
            }
            if(log_entries_[prev_log_index].term != prev_log_term){
                return false;
            }
            return true;
        }

        //will remove entries with index equal to or larger than index_to_remove
        void clean_log_entries_out_of_sync(int index_to_remove){
            log_entries_.erase(log_entries_.begin() + index_to_remove, log_entries_.end());
        }

        void append_log_entries(int last_log_index, const std::vector<std::string>& new_entries, const std::vector<int>& new_entry_terms){

            assert(new_entries.size() == new_entry_terms.size());
            for(size_t i = 0; i < new_entries.size(); ++i){
                size_t index_in_log_entries = i + last_log_index + 1;

                if( index_in_log_entries > log_entries_.size() - 1){
                    //new entry
                    log_entries_.push_back(LogEntry{(int)index_in_log_entries, new_entry_terms[i], new_entries[i]});
                }
                else if(log_entries_[index_in_log_entries].term == new_entry_terms[i]){
                    //already exists and terms match
                }
                else {
                    //does not match, need to cleanup
                    clean_log_entries_out_of_sync(index_in_log_entries);
                    log_entries_.push_back(LogEntry{(int)index_in_log_entries, new_entry_terms[i], new_entries[i]});
                }

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
        std::vector<LogEntry> log_entries_;
        time_t last_heart_beat_received_;
        int votes_collected_;
        int voted_for_; // -1 means did not vote yet

        //state for commits and index

        int commit_index_;
        int last_applied_;
        std::map<int, int> next_index_; //map from node id to next index
        std::map<int, int> matched_index_; // map from node id to last matched index

        Message serialize_raft_message(const RaftMessage& raft_message){
            return Message(raft_message.serialize());
        }

};


