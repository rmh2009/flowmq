#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>
#include <mymq/log_entry.hpp>
#include <mymq/message.hpp>
#include <mymq/flow_message.pb.h>

// two types of Raft RPC message, RequestVote and 
// AppendEntries, each type could be either request 
// or response.

namespace flowmq{

using RequestVoteRequestType = FlowMessage_RequestVoteRequest;
using RequestVoteResponseType = FlowMessage_RequestVoteResponse;
using AppendEntriesRequestType = FlowMessage_AppendEntriesRequest;
using AppendEntriesResponseType = FlowMessage_AppendEntriesResponse;

using ClientPutMessageType = FlowMessage_ClientPutMessage;
using ClientCommitMessageType = FlowMessage_ClientCommitMessage;
using ServerSendMessageType = FlowMessage_ServerSendMessage;
using ClientOpenQueueRequestType = FlowMessage_ClientOpenQueue;
using ConsumerDisconnectedType = FlowMessage_ConsumerDisconnected;

// Wrapper class on top of the Protobuf class flowmq::FlowMessage. 
// Provides utility functions for interacting with the lower level 
// flowmq::Message class.

class RaftMessage {
    public:
        enum MessageType {
            REQUEST_VOTE_REQUEST    = FlowMessage::REQUEST_VOTE_REQUEST   ,
            REQUEST_VOTE_RESPONSE   = FlowMessage::REQUEST_VOTE_RESPONSE  ,
            APPEND_ENTRIES_REQUEST  = FlowMessage::APPEND_ENTRIES_REQUEST ,
            APPEND_ENTRIES_RESPONSE = FlowMessage::APPEND_ENTRIES_RESPONSE,
            CLIENT_PUT_MESSAGE      = FlowMessage::CLIENT_PUT_MESSAGE     ,
            CLIENT_COMMIT_MESSAGE   = FlowMessage::CLIENT_COMMIT_MESSAGE  ,
            SERVER_SEND_MESSAGE     = FlowMessage::SERVER_SEND_MESSAGE    ,
            CLIENT_OPEN_QUEUE       = FlowMessage::CLIENT_OPEN_QUEUE      ,
            CONSUMER_DISCONNECTED   = FlowMessage::CONSUMER_DISCONNECTED  ,
            UNKNOWN                 = FlowMessage::UNKNOWN                
        };

        RaftMessage()
        {};

        void loadVoteRequest(RequestVoteRequestType req){
            flow_message_.set_type(FlowMessage::REQUEST_VOTE_REQUEST);
            (*flow_message_.mutable_request_vote_request()) = std::move(req);
        }

        void loadVoteResult(RequestVoteResponseType resp){
            flow_message_.set_type(FlowMessage::REQUEST_VOTE_RESPONSE);
            (*flow_message_.mutable_request_vote_response()) = std::move(resp);
        }

        void loadAppendEntriesRequest(AppendEntriesRequestType req){
            flow_message_.set_type(FlowMessage::APPEND_ENTRIES_REQUEST);
            (*flow_message_.mutable_append_entries_request()) = std::move(req);
        }

        void loadAppendEntriesResponse(AppendEntriesResponseType resp){
            flow_message_.set_type(FlowMessage::APPEND_ENTRIES_RESPONSE);
            (*flow_message_.mutable_append_entries_response()) = std::move(resp);
        }

        void loadClientPutMessageRequest(ClientPutMessageType put){
            flow_message_.set_type(FlowMessage::CLIENT_PUT_MESSAGE);
            (*flow_message_.mutable_client_put_message()) = std::move(put);
        }

        void loadClientCommitMessageRequest(ClientCommitMessageType commit){
            flow_message_.set_type(FlowMessage::CLIENT_COMMIT_MESSAGE);
            (*flow_message_.mutable_client_commit_message()) = std::move(commit);
        }

        void loadClientOpenQueueRequest(ClientOpenQueueRequestType open){
            flow_message_.set_type(FlowMessage::CLIENT_OPEN_QUEUE);
            (*flow_message_.mutable_client_open_queue()) = std::move(open);
        }

        void loadServerSendMessageRequest(ServerSendMessageType send){
            flow_message_.set_type(FlowMessage::SERVER_SEND_MESSAGE);
            (*flow_message_.mutable_server_send_message()) = std::move(send);
        }

        const AppendEntriesRequestType& get_append_request() const{
            return flow_message_.append_entries_request();
        }

        const AppendEntriesResponseType& get_append_response() const{
            return flow_message_.append_entries_response();
        }

        const RequestVoteRequestType& get_vote_request() const{
            return flow_message_.request_vote_request();
        }

        const RequestVoteResponseType& get_vote_response() const{
            return flow_message_.request_vote_response();
        }

        const ClientPutMessageType& get_put_message_request() const{
            return flow_message_.client_put_message();
        }

        const ClientCommitMessageType& get_commit_message_request() const{
            return flow_message_.client_commit_message();
        }

        const ClientOpenQueueRequestType get_open_queue_request() const{
            return flow_message_.client_open_queue();
        }

        const ServerSendMessageType& get_server_send_essage() const{
            return flow_message_.server_send_message();
        }


        MessageType type() const{
            return static_cast<MessageType>(flow_message_.type());
        }

        // Should only be used for debugging 
        std::string serialize() const {
            if(flow_message_.type() == FlowMessage::UNKNOWN){
                std::cout << "ERROR! message type is not set!" << flow_message_.DebugString() << '\n';
                throw(std::runtime_error("Message type not set!"));
            }
            return DebugString();
        }

        // Should only be used for debugging 
        std::string DebugString() const { 
            return flow_message_.DebugString();
        }

        // Should only be used for debugging 
        static RaftMessage deserialize(const std::string& str){
            RaftMessage msg;
            msg.flow_message_.ParseFromString(str);
            
            return msg;
        }

        void serialize_to_message(Message* msg) const {

            size_t msg_len = flow_message_.ByteSizeLong();
            msg->set_body_length(msg_len);
            flow_message_.SerializeToArray(msg->body(), msg_len);

        }

        Message serialize_as_message() const{
            Message msg;
            serialize_to_message(&msg);
            return msg;
        }

        static RaftMessage deserialize_from_message(const ::flowmq::Message& message){
            RaftMessage raft_msg;
            raft_msg.flow_message_.ParseFromArray(message.body(), message.body_length());
            return raft_msg;
        }


    private:

        flowmq::FlowMessage flow_message_;
        
};

} // namespace flowmq


