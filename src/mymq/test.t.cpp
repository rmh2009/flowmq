#include <mymq/raft_message.hpp>

int main(){


    auto fun = [](const RaftMessage& msg){

        auto str = msg.serialize();
        std::cout << "serialized raft message            : " << str << '\n';

        auto new_msg = RaftMessage::deserialize(str);
        std::cout << "deserialized then serialized again : " << new_msg.serialize() << '\n';
    };


    {
        RaftMessage msg; 
        msg.loadAppendEntriesRequest(
                0, 1, 100, 200, {}, 5);
        fun(msg);
    }
    {
        RaftMessage msg; 
        msg.loadAppendEntriesRequest(
                0, 1, 100, 200, {"test1", "test2 abc", " sdfsdf!  sdfsdf"}, 5);
        fun(msg);
    }

    {
        RaftMessage msg; 
        msg.loadAppendEntriesRequest(
                0, 1, 100, 200, {"naughty\0 sdfd", "test2 abc", " sdfsdf!  sdfsdf"}, 5);
        fun(msg);
    }


    {
        RaftMessage msg; 
        msg.loadVoteRequest(
                0, 1, 100, 200);
        fun(msg);
    }
    {
        RaftMessage msg; 
        msg.loadVoteResult(
               1, true);
        fun(msg);
    }
    {
        try{
        //error
        RaftMessage msg; 
        fun(msg);
        }
        catch( const std::exception& e){
            std::cout << e.what() << '\n';
        }
    }



}
