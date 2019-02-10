#pragma once 

#include <vector>
#include <flowmq/raft_message.hpp>

namespace flowmq{

// Abstract class for storage of entry logs 
// and metadata in a cluster node
class ClusterNodeStorageInterface { 

    public:
        virtual int load_log_entry_from_file(std::vector<LogEntry>* entries) = 0;
        virtual int load_metadata_from_file(LogEntryMetaData* metadata) = 0;

        virtual void store_log_entries_and_metadata(const std::vector<LogEntry>& entries, 
                int start_entry_index, 
                int stop_entry_index,
                const LogEntryMetaData& metadata) = 0;

        virtual ~ClusterNodeStorageInterface(){};
};


class ClusterNodeStorage : public ClusterNodeStorageInterface {
    public:

        ClusterNodeStorage(int partition_id, int node_id);

        int  load_log_entry_from_file(std::vector<LogEntry>* entries) override;
        int  load_metadata_from_file(LogEntryMetaData* metadata) override;

        void store_log_entries_and_metadata(const std::vector<LogEntry>& entries, 
                int start_entry_index, 
                int stop_entry_index,
                const LogEntryMetaData& metadata) override;

    private:
        std::string storage_log_entry_filename_;
        std::string storage_metadata_file_name_;

};



} // namespace flowmq
