#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>

// two types of Raft RPC message, RequestVote and 
// AppendEntries, each type could be either request 
// or response.
class RaftMessage {
    public:
        enum TYPE {

            RequestVoteRequest = 0, 
            RequestVoteResponse = 1, 
            AppendEntriesRequest = 2, 
            AppendEntriesResponse = 3,
            Unknown = 99
        };

        RaftMessage():
            type_(Unknown)
    {};

        void loadVoteRequest(
                int term, 
                int candidate_id,
                int last_log_index,
                int last_log_term){
            type_ = RequestVoteRequest;
            term_ = term;
            candidate_id_ = candidate_id;
            last_log_index_ = last_log_index;
            last_log_term_ = last_log_term;
        }

        void loadVoteResult(
                int term, 
                bool vote_granted
                ){
            type_ = RequestVoteResponse;
            vote_result_term_ = term;
            vote_result_term_granted_ = vote_granted? 1 : 0;
        }

        void loadAppendEntriesRequest(
                int term,
                int leader_id,
                int prev_log_index,
                int prev_log_term,
                std::vector<std::string>&& entries, 
                int leader_commit){
            type_ = AppendEntriesRequest;
            term_ = term; 
            leader_id_ = leader_id; 
            prev_log_index_ = prev_log_index;
            prev_log_term_ = prev_log_term;
            entries_ = std::move(entries);
            leader_commit_ = leader_commit;
        }

        std::string serialize() const {

            std::stringstream ss;

            ss << type_ << ' ';
            ss << term_ << ' ';
            switch(type_) {
            case RequestVoteRequest : 
                ss << candidate_id_ << ' ';
                ss << last_log_index_ << ' ';
                ss << last_log_term_<< ' ';

                break;
            case RequestVoteResponse:
                ss << vote_result_term_ << ' ';
                ss << vote_result_term_granted_ << ' ';

                break;
            case AppendEntriesRequest:
                ss << leader_id_ << ' ';
                ss << prev_log_index_ << ' ';
                ss << prev_log_term_ << ' ';
                ss << leader_commit_ << ' ';
                for ( auto& entry : entries_){
                    ss << entry.size() << ' ';
                    ss << entry;
                }

                break;
            default :
                throw std::runtime_error("unsupported raft message type!");
            }

            return ss.str();
        }

        static RaftMessage deserialize(const std::string& str){

            std::stringstream ss(str);

            RaftMessage msg;
            ss >> msg.type_;
            ss >> msg.term_;

            switch(msg.type_){
            case RequestVoteRequest : 
                ss >> msg.candidate_id_ ;
                ss >> msg.last_log_index_ ;
                ss >> msg.last_log_term_ ;

                break;
            case RequestVoteResponse:
                ss >> msg.vote_result_term_ ;
                ss >> msg.vote_result_term_granted_ ;

                break;
            case AppendEntriesRequest:
                ss >> msg.leader_id_ ;
                ss >> msg.prev_log_index_ ;
                ss >> msg.prev_log_term_ ;
                ss >> msg.leader_commit_ ;
                int next_string_size;
                while(ss >> next_string_size && !ss.fail() && !ss.eof()){
                    char space;
                    ss.read(&space, 1);
                    msg.entries_.emplace_back();
                    msg.entries_.back().resize(next_string_size);
                    ss.read(&msg.entries_.back()[0], next_string_size);
                }

                break;
            default :
                std::cout << "ERROR unkown message type " << msg.type_ << '\n';
                throw std::runtime_error("unsupported raft message type!");
            }

            return msg;
        }

    private:

        // generic
        int  type_;
        int term_;
        
        // RequestVoteRequest
        int candidate_id_;
        int last_log_index_;
        int last_log_term_;

        // RequestVoteResponse
        int vote_result_term_;
        int vote_result_term_granted_;

        // AppendEntriesRequest
        int leader_id_;
        int prev_log_index_;
        int prev_log_term_;
        int leader_commit_;
        std::vector<std::string> entries_;

        // AppendEntriesResponse
        int append_result_term_;
        int append_result_success_;
};



