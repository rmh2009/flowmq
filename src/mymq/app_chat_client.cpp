#include <mymq/chat_client.hpp>
#include <iostream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]){

    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: chat_client <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(argv[1], argv[2]);
        ChatClient client(io_context, endpoints);

        std::cout << "client started \n";

        std::thread t([&io_context](){ io_context.run(); });

        char line[Message::DATA_LENGTH + 1];
        while (std::cin.getline(line, sizeof(line)))
        {
            Message msg;
            std::memcpy(msg.data(), line, std::strlen(line));
            client.write_message(msg);
        }
        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }


    return 0;
}
