#pragma once
#include <string>

// Super simplified message
class Message{

    public:
        Message():
            body_length_(0)
        {
        }

        Message(const std::string& msg){
            body_length_ = msg.size();
            std::string header_length_str = std::to_string(body_length_);
            std::fill_n(header(), header_length(), '\0');
            memcpy(header(), header_length_str.c_str(), header_length_str.size());
            memcpy(body(), msg.c_str(), msg.size());
        }

        enum { HEADER_LENGTH = 20,
               MAX_BODY_LENGTH = 1000};

        const char* body() const {
            return &data_[HEADER_LENGTH];
        }

        char* body() {
            return &data_[HEADER_LENGTH];
        }

        const char* header() const {
            return &data_[0];
        }

        char* header(){
            return &data_[0];
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

    private:
        size_t body_length_;
        char data_[HEADER_LENGTH + MAX_BODY_LENGTH];
};
