
#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>



int main(){
    
    boost::asio::io_context io; 
    boost::asio::deadline_timer t(io, boost::posix_time::seconds(4));
    t.async_wait([](const boost::system::error_code& err){std::cout << "timer 1 done\n";});

    boost::asio::deadline_timer t2(io, boost::posix_time::seconds(5));
    t2.async_wait([](const boost::system::error_code& err){std::cout << "timer 2 done\n";});
    std::cout << "hello!\n";

    io.run();

    return 0;
}
