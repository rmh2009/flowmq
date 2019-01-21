#pragma once
#include <string>
#include <vector>

// Super simplified message
namespace flowmq{

// Low level message to be used with Session class. 
// Mainly used as a buffer when reading and writing 
// from asio socket.
// To reduce copying, this class is move only.

class Message{

    public:
        Message():
            body_length_(0)
        {
            data_buffer_.resize(HEADER_LENGTH + MAX_BODY_LENGTH);
        }
        Message(Message&& msg){
            body_length_ = msg.body_length_;
            data_buffer_ = std::move(msg.data_buffer_);
        }

        enum { HEADER_LENGTH = 20,
               MAX_BODY_LENGTH = 1000};

        const char* body() const {
            return &data_buffer_.data()[HEADER_LENGTH];
        }

        char* body() {
            return &data_buffer_[HEADER_LENGTH];
        }

        const char* header() const {
            return data_buffer_.data();
        }

        char* header(){
            return data_buffer_.data();
        }

        size_t body_length() const {
            return body_length_;
        }

        size_t header_length() const {
            return HEADER_LENGTH;
        }

        void decode_length(){
            body_length_ = std::atoi(header());
        }

        void set_body_length(size_t length){
            assert(length <= MAX_BODY_LENGTH);
            body_length_ = length;
            std::string header_length_str = std::to_string(body_length_);
            std::fill_n(header(), header_length(), '\0');
            memcpy(header(), header_length_str.c_str(), header_length_str.size());
        }

    private:
        // no copy
        Message(const Message& msg) = delete;
        Message& operator=(const Message& msg) = delete;

        size_t body_length_;
        std::vector<char> data_buffer_;
};

}
