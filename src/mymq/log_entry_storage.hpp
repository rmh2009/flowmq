#pragma once
#include <fstream>
#include <iostream>

#include <mymq/log_entry.hpp>

class MetadataStorage{
    public:
        static int save_metadata_to_file(const std::string& filename, const LogEntryMetaData& metadata);
        static int load_metadata_from_file(const std::string& filename, LogEntryMetaData* metadata);
};

class LogEntryStorage{
    public:
        static int save_log_entry_to_file(const std::string& filename, const std::vector<LogEntry>& entries);
        static int append_log_entry_to_file(const std::string& filename, const std::vector<LogEntry>& entries);
        static int load_log_entry_from_file(const std::string& filename, std::vector<LogEntry>* entries);
};
