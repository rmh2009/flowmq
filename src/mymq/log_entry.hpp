#pragma once

#include <string>
#include <vector>
#include <sstream>

// Class for Log Entry


struct LogEntryMetaData{
    int last_committed;

    std::string serialize() const {
        return std::to_string(last_committed);
    }

    static LogEntryMetaData deserialize(const std::string& str){

        LogEntryMetaData metadata;
        metadata.last_committed = stoi(str);
        return metadata;
    }
};

inline
bool operator==(const LogEntryMetaData& l, const LogEntryMetaData& r){
    return l.last_committed == r.last_committed;
}

struct LogEntry{
    enum OPERATION {
        ADD = 0, 
        COMMIT = 1
    };

    int index;
    int term;

    int message_id;
    int operation; 
    std::string message;

    std::string serialize() const {

        std::stringstream ss;
        ss << index << ' ';
        ss << term << ' ';
        ss << message_id << ' ';
        ss << operation << ' ';
        ss << message.size() << ' ';
        ss << message ;
        
        return ss.str();
    }

    static LogEntry deserialize(const std::string& str){

        LogEntry entry;
        std::stringstream ss(str);
        ss >> entry.index ; 
        ss >> entry.term ; 
        ss >> entry.message_id; 
        ss >> entry.operation;
        int msg_size;
        ss >> msg_size;
        char space;
        ss.read(&space, 1);
        entry.message.resize(msg_size);
        ss.read(&entry.message[0], msg_size);

        return entry;

    }
};

inline
bool operator==(const LogEntry& l, const LogEntry& r){
    if(l.index != r.index) return false;
    if(l.message != r.message) return false;
    if(l.message_id != r.message_id) return false;
    if(l.operation != r.operation) return false;
    if(l.term != r.term) return false;
    return true;
}



