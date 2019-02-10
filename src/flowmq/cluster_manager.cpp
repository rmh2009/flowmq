#include <flowmq/cluster_manager.hpp>
#include <flowmq/logging.hpp>

namespace flowmq{

ClusterManager::ClusterManager(boost::asio::io_context& io_context, 
        const tcp::endpoint& endpoint,  // this instance endpoint
        const std::vector<std::pair<int, tcp::resolver::results_type>>& endpoints // other nodes
        ):
    io_context_(io_context),
    acceptor_(io_context, endpoint),
    incoming_sessions_count_(0),
    endpoints_(endpoints)
{

}

void ClusterManager::start(){
    LOG_INFO << "end points " << endpoints_.size() << '\n';
    accept_new_connection();
    for(auto& p : endpoints_){
        endpoint_id_map_[p.first] = p.second;
    }
    for(auto& p : endpoints_){
        connect(p.first);
    }
}
void ClusterManager::broad_cast(Message msg){
    LOG_INFO << "current outoing connections: " << outgoing_sessions_.size() << '\n';
    // here this needs to be shared since it is sent to multiple receivers.
    auto msg_shared = std::make_shared<Message>(std::move(msg));
    io_context_.post([msg_shared, this](){
        for(auto& session : outgoing_sessions_){
            session.second -> write_message(msg_shared); 
        }
    });
}

void ClusterManager::write_message(int endpoint_id, Message msg){

    auto msg_shared = std::make_shared<Message>(std::move(msg));
    io_context_.post([msg_shared, endpoint_id, this](){
        if(outgoing_sessions_.count(endpoint_id) != 0){
            outgoing_sessions_[endpoint_id] -> write_message(msg_shared);
        }
    });
}

// By default all sessions created in this manager uses the same io_context, 
// and is single threaded, so there is no need to lock between this read 
// handler functions and the other write functions (which uses io_context_.post)
void ClusterManager::connect(int endpoint_id){
    if(outgoing_sessions_.count(endpoint_id) 
            && outgoing_sessions_[endpoint_id] -> get_status() == Session::CONNECTED){
        return;
    }

    LOG_INFO << "connecting to " << endpoint_id << "... \n";
    auto socket = std::make_shared<tcp::socket>(io_context_);
    boost::asio::async_connect(*socket, endpoint_id_map_[endpoint_id], [this, socket, endpoint_id](
                const boost::system::error_code& ec,
                tcp::endpoint
                ){

            if(!ec){
            LOG_INFO << endpoint_id << " connected\n";

            auto session = std::make_shared<Session>(std::move(*socket));
            session -> register_handler([](const Message& msg){
                    LOG_INFO << "Warning Not supposed to get incoming message from this session! messag:" 
                    << std::string(msg.body(), msg.body_length()) << '\n';
                    });

            session -> register_disconnect_handler([this, endpoint_id](){
                    LOG_INFO << "Remote session closed, exiting ..." ;
                    auto timer = std::make_shared<boost::asio::deadline_timer>(io_context_);
                    timer -> expires_from_now(boost::posix_time::seconds(2));
                    timer -> async_wait([timer, endpoint_id, this](boost::system::error_code const&){
                            connect(endpoint_id);
                            });                    
                    });

            session -> start_read();
            outgoing_sessions_[endpoint_id] = session;

            }
            else{

                std::time_t result = std::time(nullptr);
                LOG_INFO << result << " ERROR endpoint not up, retrying ...\n";
                auto timer = std::make_shared<boost::asio::deadline_timer>(io_context_);
                timer -> expires_from_now(boost::posix_time::seconds(2));
                timer -> async_wait([timer, endpoint_id, this](boost::system::error_code const&){
                        connect(endpoint_id);
                        });                    
            }

    });
}


void ClusterManager::accept_new_connection(){
    acceptor_.async_accept([this](boost::system::error_code error, tcp::socket socket){

            if(!error){

            LOG_INFO << "got new connection! current connections including this is " 
            << incoming_sessions_.size() + 1 << '\n';

            auto new_session = std::make_shared<Session>(std::move(socket));

            new_session -> register_handler([this](const Message& msg){
                    //LOG_INFO << "#] " << std::string(msg.body(), msg.body_length()) << '\n';
                    handler_(msg);
                    });

            int id = incoming_sessions_count_++;

            new_session -> register_disconnect_handler([this, id](){
                    LOG_INFO << "Removing session " << id << " from chat room!\n";
                    auto to_remove = incoming_sessions_.find(id);
                    if(to_remove != incoming_sessions_.end()){
                    incoming_sessions_.erase(incoming_sessions_.find(id));
                    }
                    else{
                    LOG_INFO << "Session " << id << " already removed!\n";
                    }
                    });

            new_session -> start_read();
            incoming_sessions_[id] = new_session;

            }
            else{
                LOG_INFO << "ERROR: error while accepting new connection : " << error << '\n';
            }

            accept_new_connection();

    });
}



}
