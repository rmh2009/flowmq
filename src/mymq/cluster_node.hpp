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

        const int HEARTBEAT_EXPIRE_SECONDS = 3; // if no heart beat received in these seconds, will start new vote
        const int HEARTBEAT_CHECK_INTERVAL = 5; // schedule interval to check heart beats

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
                cur_term_(0),
                voted_for_(-1)
        {
            //initialize random seed
            
            auto ut = boost::posix_time::microsec_clock::universal_time();
            auto duration = ut.time_of_day();
            std::cout << "setting random seed according to microseconds" << duration.total_microseconds() % 1000000
                << " and pid " << getpid() << '\n';
            std::srand(getpid() + duration.total_microseconds() % 1000000);

            log_entries_.push_back({0, 0, ""});
            cluster_manager_.start();

            //scheduler to check hearbeat and initiate vote periodically
            start_vote_scheduler();
            start_send_hearbeat();
            cluster_manager_.register_handler(std::bind(&ClusterNode::message_handler, this, std::placeholders::_1));
        }

        ClusterNode() = delete;
        ClusterNode(const ClusterNode& ) = delete;
        ClusterNode& operator=(const ClusterNode&) = delete;
        ClusterNode(ClusterNode&& ) = delete;

    private:
        //this will send heart beats to all followers if current state is leader
        void start_send_hearbeat(){
            
            std::cout << "sending heartbeat (if leader) ... ... \n";

            heartbeat_timer_.expires_from_now(boost::posix_time::seconds(HEARTBEAT_EXPIRE_SECONDS));
            heartbeat_timer_.async_wait([this](boost::system::error_code ){

                    if(state_ == LEADER){
                    std::cout << " *************** node " << node_id_ << " : I'm the leader, checking heartbeat now! *************\n";
                    RaftMessage msg;
                    int last_log_term = log_entries_.back().term;
                    int last_log_index = log_entries_.back().index;
                    msg.loadAppendEntriesRequest(AppendEntriesRequestType(
                                {cur_term_, node_id_, last_log_term, last_log_index, std::vector<std::string>(), last_log_index}));

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

        // handles all incoming messages from the cluster
        // this is run in the io_context thread.
        void message_handler(const Message& msg){

            RaftMessage raft_msg = RaftMessage::deserialize(std::string(msg.body(), msg.body_length()));

            switch(raft_msg.type()){
                case RaftMessage::AppendEntriesRequest : 
                    {
                        const AppendEntriesRequestType& req = raft_msg.get_append_request();
                        if(req.term < log_entries_.back().term){
                            std::cout << "ERROR: Received expired heart beat from leader! starting vote!";
                            state_ = CANDIDATE;
                            return;
                        }

                        if(state_ == CANDIDATE || state_ == LEADER){
                            std::cout << "Received new leader id " << req.leader_id << "\n";
                            state_ = FOLLOWER;

                            //TODO append logs from leader
                            return;
                        }

                        //update last_heart_beat_received_
                        std::time(&last_heart_beat_received_);
                        cur_term_ = req.term;


                        break;
                    }
                case RaftMessage::AppendEntriesResponse : 
                    {
                        std::cout << "ERROR : not handling this type of response yet.\n";
                        break;
                    }
                case RaftMessage::RequestVoteRequest:
                    {
                        const RequestVoteRequestType& req = raft_msg.get_vote_request();

                        int candidate_node_id = req.candidate_id;
                        int request_term = req.term;

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
                default :
                    break;
            }
        }

        int node_id_;
        int total_nodes_;
        boost::asio::io_context& io_context_;
        ClusterManager cluster_manager_;
        State state_;
        boost::asio::deadline_timer vote_timer_;
        boost::asio::deadline_timer heartbeat_timer_;

        int cur_term_;
        std::vector<LogEntry> log_entries_;
        time_t last_heart_beat_received_;
        int votes_collected_;
        int voted_for_; // -1 means did not vote yet

        Message serialize_raft_message(const RaftMessage& raft_message){
            return Message(raft_message.serialize());
        }

};


