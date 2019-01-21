#include <ctime>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using boost::asio::ip::tcp;

std::string get_date_string(){

    std::time_t now = std::time(0);
    return std::ctime(&now);
    
}

int main(){
    try{

        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 13));
        
        for(;;){
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            std::string str = get_date_string();

            boost::system::error_code err;
            boost::asio::write(socket, boost::asio::buffer(str), err);
            std::cout << "wrote data \n";
        }

    }
    catch(const std::exception& e){

    }

    return 0;
}


