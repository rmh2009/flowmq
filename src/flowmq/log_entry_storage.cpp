#include <flowmq/log_entry_storage.hpp>
#include <flowmq/logging.hpp>

namespace flowmq{

int MetadataStorage::save_metadata_to_file(
        const std::string& filename, 
        const LogEntryMetaData& metadata){

    std::ofstream f(filename, std::ios::out);
    if(!f.is_open()) { 
        return 1;}
    f << metadata.serialize() << ' ';
    f.close();
    return 0;
}

int MetadataStorage::load_metadata_from_file(const std::string& filename, LogEntryMetaData* metadata){
    std::ifstream f(filename);
    f >> (metadata->last_committed);
    f.close();
    return 0;
}

int LogEntryStorage::save_log_entry_to_file(const std::string& filename, const std::vector<LogEntry>& entries){

    std::ofstream f(filename, std::ios::out);
    if(!f.is_open()) return 1;

    for(auto& entry : entries){
        std::string temp = entry.SerializeAsString();
        f << temp.size() << ' ' << temp;
    }

    f.close();
    return 0;

}

int LogEntryStorage::append_log_entry_to_file(const std::string& filename, const std::vector<LogEntry>& entries){

    std::ofstream f(filename, std::ios::app);
    if(!f.is_open()) return 1;

    for(auto& entry : entries){
        std::string temp = entry.SerializeAsString();
        f << temp.size() << ' ' << temp;
    }
    f.close();
    return 0;
}

int LogEntryStorage::load_log_entry_from_file(const std::string& filename, std::vector<LogEntry>* entries){

    std::ifstream f(filename);
    int entry_size = 0;
    char c; 
    std::string temp;

    while(f >> entry_size && f.good()){
        f.read(&c, 1);
        temp.resize(entry_size);
        f.read(&temp[0], entry_size);
        LOG_INFO << "read message :" << temp << '\n';
        LogEntry temp_entry;
        temp_entry.ParseFromString(temp);
        entries->push_back(std::move(temp_entry));
    }

    if(f.eof()) {
        //normal
        f.close();
        return 0;
    }
    else {
        //unknown error 
        f.close();
        return 1;
    }   
}

} // namespace flowmq

