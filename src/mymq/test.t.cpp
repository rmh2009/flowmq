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
        AppendEntriesRequestType append_req({0, 1, 100, 200, std::vector<std::string>(), 5});
        msg.loadAppendEntriesRequest(append_req);
                
        fun(msg);
    }
    {
        RaftMessage msg; 
        AppendEntriesRequestType append_req({0, 1, 100, 200, std::vector<std::string>({"test1", "test2 abc", " sdfsdf!  sdfsdf"}), 5});
        msg.loadAppendEntriesRequest(append_req);
        fun(msg);
    }

    {
        RaftMessage msg; 
        AppendEntriesRequestType append_req({0, 1, 100, 200, std::vector<std::string>({"naughty\0 sdfd", "test2 abc", " sdfsdf!  sdfsdf"}), 5});
        msg.loadAppendEntriesRequest(append_req);
        fun(msg);
    }


    {
        RaftMessage msg; 
        msg.loadVoteRequest(RequestVoteRequestType{0, 1, 100, 200});
        fun(msg);
    }
    {
        RaftMessage msg; 
        msg.loadVoteResult(RequestVoteResponseType{1, true});
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
