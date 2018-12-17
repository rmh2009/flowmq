#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>

// two types of Raft RPC message, RequestVote and 
// AppendEntries, each type could be either request 
// or response.
//

struct RequestVoteRequestType{

    int term;
    int candidate_id;
    int last_log_index;
    int last_log_term;
};

struct RequestVoteResponseType{
    int term;
    int vote_result_term_granted;
};

struct AppendEntriesRequestType{
    int term;
    int leader_id;
    int prev_log_index;
    int prev_log_term;
    std::vector<std::string> entries;
    std::vector<int> entry_terms;
    int leader_commit;
};

struct AppendEntriesResponseType{
    int term;
    int follower_id;
    int append_result_success;
    int last_index_synced;
};

//message for message queue client to put a message onto the queue
struct ClientPutMessageType{
    std::string message;
};

class RaftMessage {
    public:
        enum TYPE {

            RequestVoteRequest = 0, 
            RequestVoteResponse = 1, 
            AppendEntriesRequest = 2, 
            AppendEntriesResponse = 3,
            ClientPutMessage = 10,
            Unknown = 99
        };

        RaftMessage():
            type_(Unknown)
    {};

        template<class... ARGS>
        void loadVoteRequest(ARGS&&... args){
            type_ = RequestVoteRequest;
            vote_request_ = RequestVoteRequestType(std::forward<ARGS>(args)...);
        }

        template<class... ARGS>
        void loadVoteResult(ARGS&&... args){
            type_ = RequestVoteResponse;
            vote_response_ = RequestVoteResponseType(std::forward<ARGS>(args)...);
        }

        template<class... ARGS>
        void loadAppendEntriesRequest(ARGS&&... args){
            type_ = AppendEntriesRequest;
            append_request_ = AppendEntriesRequestType(std::forward<ARGS>(args)...);
        }

        template<class... ARGS>
        void loadAppendEntriesResponse(ARGS&&... args){
            type_ = AppendEntriesResponse;
            append_response_ = AppendEntriesResponseType(std::forward<ARGS>(args)...);
        }

        template<class... ARGS>
        void loadClientPutMessageRequest(ARGS&&... args){
            type_ = ClientPutMessage;
            client_put_message_ = ClientPutMessageType(std::forward<ARGS>(args)...);
        }



        int type() const{
            return type_;
        }

        std::string serialize() const {

            std::stringstream ss;

            ss << type_ << ' ';
            switch(type_) {
            case RequestVoteRequest : 
                ss << vote_request_.term << ' ';
                ss << vote_request_.candidate_id << ' ';
                ss << vote_request_.last_log_index << ' ';
                ss << vote_request_.last_log_term<< ' ';
                break;
            case RequestVoteResponse:
                ss << vote_response_.term << ' ';
                ss << vote_response_.vote_result_term_granted << ' ';

                break;
            case AppendEntriesRequest:
                ss << append_request_.term << ' ';
                ss << append_request_.leader_id << ' ';
                ss << append_request_.prev_log_index << ' ';
                ss << append_request_.prev_log_term << ' ';
                ss << append_request_.leader_commit << ' ';
                ss << append_request_.entries.size() << ' ';
                for ( auto& entry : append_request_.entries){
                    ss << entry.size() << ' ';
                    ss << entry;
                }
                for ( auto& entry_term : append_request_.entry_terms){
                    ss << entry_term << ' ';
                }
                break;
            case AppendEntriesResponse:
                ss << append_response_.term << ' '; 
                ss << append_response_.follower_id << ' '; 
                ss << append_response_.append_result_success << ' ';
                ss << append_response_.last_index_synced << ' ';
                break;

            case ClientPutMessage : 
                ss << client_put_message_.message.size() << ' ';
                ss << client_put_message_.message;
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

            switch(msg.type_){
            case RequestVoteRequest : 
                ss >> msg.vote_request_.term;
                ss >> msg.vote_request_.candidate_id ;
                ss >> msg.vote_request_.last_log_index ;
                ss >> msg.vote_request_.last_log_term ;

                break;
            case RequestVoteResponse:
                ss >> msg.vote_response_.term;
                ss >> msg.vote_response_.vote_result_term_granted ;

                break;
            case AppendEntriesRequest:
                ss >> msg.append_request_.term;
                ss >> msg.append_request_.leader_id ;
                ss >> msg.append_request_.prev_log_index ;
                ss >> msg.append_request_.prev_log_term ;
                ss >> msg.append_request_.leader_commit ;
                int entries_count;
                ss >> entries_count;
                int next_string_size;
                for(int i = 0; i < entries_count; ++i){
                    ss >> next_string_size;
                    char space;
                    ss.read(&space, 1);
                    msg.append_request_.entries.emplace_back();
                    msg.append_request_.entries.back().resize(next_string_size);
                    ss.read(&msg.append_request_.entries.back()[0], next_string_size);
                }
                for(int i = 0; i < entries_count; ++i){
                    int term; 
                    ss >> term;
                    msg.append_request_.entry_terms.push_back(term);
                }
                break;
            case AppendEntriesResponse:
                ss >> msg.append_response_.term;
                ss >> msg.append_response_.follower_id;
                ss >> msg.append_response_.append_result_success;
                ss >> msg.append_response_.last_index_synced;
                break;
            case ClientPutMessage:
                int msg_size;
                ss >> msg_size;
                char space;
                ss.read(&space, 1);
                msg.client_put_message_.message.resize(msg_size);
                ss.read(&msg.client_put_message_.message[0], msg_size);
                break;
            default :
                std::cout << "ERROR unkown message type " << msg.type_ << '\n';
                throw std::runtime_error("unsupported raft message type!");
            }

            return msg;
        }

        const AppendEntriesRequestType& get_append_request() const{
            return append_request_;
        }

        const AppendEntriesResponseType& get_append_response() const{
            return append_response_;
        }

        const RequestVoteRequestType& get_vote_request() const{
            return vote_request_;
        }

        const RequestVoteResponseType& get_vote_response() const{
            return vote_response_;
        }
        const ClientPutMessageType& get_put_message_request() const{
            return client_put_message_;
        }


    private:

        // generic
        int  type_;

        AppendEntriesRequestType append_request_;
        AppendEntriesResponseType append_response_;
        RequestVoteRequestType vote_request_;
        RequestVoteResponseType vote_response_;
        ClientPutMessageType client_put_message_;
        
};



