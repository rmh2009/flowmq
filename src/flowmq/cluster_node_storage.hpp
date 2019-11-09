#pragma once

#include <flowmq/raft_message.hpp>
#include <vector>

namespace flowmq {

// Abstract class for storage of entry logs
// and metadata in a cluster node
class ClusterNodeStorageInterface {
 public:
  virtual int load_log_entry_from_file(std::vector<LogEntry>* entries) = 0;
  virtual int load_metadata_from_file(LogEntryMetaData* metadata) = 0;

  virtual void store_log_entries_and_metadata(
      const std::vector<LogEntry>& entries, int start_entry_index,
      int stop_entry_index, const LogEntryMetaData& metadata) = 0;

  virtual ~ClusterNodeStorageInterface(){};
};

class ClusterNodeStorage : public ClusterNodeStorageInterface {
 public:
  ClusterNodeStorage(int partition_id, int node_id);

  int load_log_entry_from_file(std::vector<LogEntry>* entries) override;
  int load_metadata_from_file(LogEntryMetaData* metadata) override;

  void store_log_entries_and_metadata(
      const std::vector<LogEntry>& entries, int start_entry_index,
      int stop_entry_index, const LogEntryMetaData& metadata) override;
  virtual ~ClusterNodeStorage();

 private:
  void consume_and_store_messages();

  std::string storage_log_entry_filename_;
  std::string storage_metadata_file_name_;
  std::vector<LogEntry> log_entry_buffers_;
  LogEntryMetaData metadata_buffer_;
  std::mutex mutex_;
  // used to cancel the writing thread.
  bool cancelled_;
  std::thread thread_;
};

// Mock storage that does not really store anything.
class ClusterNodeStorageMock : public ClusterNodeStorageInterface {
 public:
  ClusterNodeStorageMock(int partition_id, int node_id)
      : partition_id_(partition_id), node_id_(node_id) {}

  int load_log_entry_from_file(std::vector<LogEntry>*) override { return 0; }
  int load_metadata_from_file(LogEntryMetaData*) override { return 0; }

  void store_log_entries_and_metadata(const std::vector<LogEntry>&, int, int,
                                      const LogEntryMetaData&) override {
    return;
  }

 private:
  int partition_id_;
  int node_id_;
};

}  // namespace flowmq
