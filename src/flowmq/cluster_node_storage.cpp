#include <flowmq/cluster_node_storage.hpp>

#include <flowmq/log_entry_storage.hpp>
namespace flowmq{

namespace{
const char* LOG_ENTRY_STORAGE_PREFIX = "storage_log_entry_";
const char* METADATA_STORAGE_PREFIX  = "storage_metadata_";
}

ClusterNodeStorage::ClusterNodeStorage(
int partition_id, int node_id){

    storage_log_entry_filename_ = LOG_ENTRY_STORAGE_PREFIX + 
        std::to_string(partition_id) + '_' + 
        std::to_string(node_id) + ".data";

    storage_metadata_file_name_ = METADATA_STORAGE_PREFIX +
        std::to_string(partition_id) + '_' + 
        std::to_string(node_id) + ".data";

}
    
int ClusterNodeStorage::load_log_entry_from_file(std::vector<LogEntry>* entries){
    return LogEntryStorage::load_log_entry_from_file(storage_log_entry_filename_, entries);
}

int ClusterNodeStorage::load_metadata_from_file(LogEntryMetaData* metadata){
    return MetadataStorage::load_metadata_from_file(storage_metadata_file_name_, metadata);
}

void ClusterNodeStorage::store_log_entries_and_metadata(const std::vector<LogEntry>& log_entries, 
                int start_entry_index, 
                int stop_entry_index,
                const LogEntryMetaData& metadata){
    assert(start_entry_index < stop_entry_index);

    std::vector<LogEntry> temp_entries(log_entries.begin() + start_entry_index, log_entries.begin() + stop_entry_index);
    LogEntryStorage::append_log_entry_to_file(storage_log_entry_filename_, temp_entries);
    MetadataStorage::save_metadata_to_file(storage_metadata_file_name_, metadata);

}

} // namespace flowmq




