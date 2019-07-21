#include <flowmq/cluster_node_storage.hpp>

#include <flowmq/log_entry_storage.hpp>
#include <unistd.h>

namespace flowmq{

namespace{
const char* LOG_ENTRY_STORAGE_PREFIX = "storage_log_entry_";
const char* METADATA_STORAGE_PREFIX  = "storage_metadata_";
}

ClusterNodeStorage::ClusterNodeStorage(
        int partition_id, int node_id):
    cancelled_(false),
    thread_(std::bind(&ClusterNodeStorage::consume_and_store_messages, this))
{

    storage_log_entry_filename_ = LOG_ENTRY_STORAGE_PREFIX + 
        std::to_string(partition_id) + '_' + 
        std::to_string(node_id) + ".data";

    storage_metadata_file_name_ = METADATA_STORAGE_PREFIX +
        std::to_string(partition_id) + '_' + 
        std::to_string(node_id) + ".data";

}

ClusterNodeStorage::~ClusterNodeStorage() {
    cancelled_ = true;
    thread_.join();
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

    std::unique_lock<std::mutex> lock(mutex_);
    log_entry_buffers_.insert(log_entry_buffers_.end(),
            log_entries.begin() + start_entry_index,
            log_entries.begin() + stop_entry_index);
    metadata_buffer_ = metadata;
}


void ClusterNodeStorage::consume_and_store_messages() {
    while(!cancelled_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            LogEntryStorage::append_log_entry_to_file(storage_log_entry_filename_, log_entry_buffers_);
            MetadataStorage::save_metadata_to_file(storage_metadata_file_name_, metadata_buffer_);
            log_entry_buffers_.clear();
        }
        // microseconds;
        usleep(100000);
    }
}

} // namespace flowmq




