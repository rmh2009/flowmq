#pragma once
#include <string>

// Super simplified message
class Message{

    public:
        enum { DATA_LENGTH = 100};

        const char* data() const {
            return &data_[0];
        }

        char* data() {
            return &data_[0];
        }

        int length() const {
            return DATA_LENGTH;
        }

    private:
        char data_[DATA_LENGTH];
};
