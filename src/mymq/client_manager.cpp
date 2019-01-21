#include <mymq/client_manager.hpp>
#include <mymq/raft_message.hpp>

namespace flowmq{

ClientManager::ClientManager(
        boost::asio::io_context& io_context, 
        const tcp::endpoint& endpoint  // this instance endpoint
        ):
    io_context_(io_context),
    acceptor_(io_context, endpoint),
    deliver_count_(0)
{
}

void ClientManager::start(){
    accept_new_connection();
}

void ClientManager::register_handler(const ReadHandler& handler){
    handler_ = handler;
}

void ClientManager::register_disconnect_handler(const ClientDisconnectedHandler& handler){
    client_disconnected_handler_ = handler;
}

//int get_next_consumer(){
//}

// Returns the client id the message was delivered to. Returns -1 on failure.
int ClientManager::deliver_one_message_round_robin(const Message& msg){

    if(consumer_client_id_array_.size() == 0) {
        std::cout << "ERROR! client_manager.cpp no active client not found!\n";
        return -1;
    }

    size_t index_in_array = deliver_count_ % consumer_client_id_array_.size();
    int client_id = consumer_client_id_array_[index_in_array];

    client_sessions_[client_id]->write_message(msg);
    ++deliver_count_;

    return client_id;

}

bool ClientManager::has_consumers() const{
    return consumer_client_id_array_.size() > 0;
}

void ClientManager::accept_new_connection(){
    acceptor_.async_accept([this](boost::system::error_code error, tcp::socket socket){

            if(!error){

            std::cout << "got new connection! current connections including this is " 
            << client_sessions_.size() + 1 << '\n';

            auto new_session = std::make_shared<Session>(std::move(socket));
            int id = std::rand();

            new_session -> register_handler([this, id](const Message& msg){
                    std::cout << "#] " << std::string(msg.body(), msg.body_length()) << '\n';
                    handle_message(msg, id);
                    });


            new_session -> register_disconnect_handler([this, id](){
                    std::cout << "Removing session " << id << " from client manager!\n";
                    auto to_remove = client_sessions_.find(id);
                    if(to_remove != client_sessions_.end()){
                    client_sessions_.erase(client_sessions_.find(id));
                    }
                    else{
                    std::cout << "Session " << id << " already removed!\n";
                    }

                    auto to_remove_from_array = std::find(consumer_client_id_array_.begin(), consumer_client_id_array_.end(), id);
                    if(to_remove_from_array != consumer_client_id_array_.end()){
                    consumer_client_id_array_.erase(to_remove_from_array);
                    }
                    else{
                    std::cout << "Session " << id << " does not exist in consumer id array!\n";
                    }

                    client_disconnected_handler_(id);

                    });

            new_session -> start_read();
            client_sessions_[id] = new_session;

            }
            else{
                std::cout << "ERROR: error while accepting new connection : " << error << '\n';
            }

            accept_new_connection();

    });
}

void ClientManager::handle_message(const Message& msg, int client_id){

    RaftMessage raft_msg = RaftMessage::deserialize(std::string(msg.body(), msg.body_length()));

    switch(raft_msg.type()){
        case FlowMessage::CLIENT_OPEN_QUEUE: 
            {
                const ClientOpenQueueRequestType& req = raft_msg.get_open_queue_request();
                std::cout << "Obtained request to open queue : " << req.DebugString() << '\n';
                consumer_client_id_array_.push_back(client_id);
                handler_(msg);
                break;
            }
        case FlowMessage::CLIENT_PUT_MESSAGE:
        case FlowMessage::CLIENT_COMMIT_MESSAGE:
            //forward to handler (in this case the cluster node)
            handler_(msg);
            break;
        default:
            throw std::runtime_error("unexpeted message type in client manager!");
            break;
    }
}

} // namespace flowmq


