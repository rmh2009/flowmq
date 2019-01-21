#include <mymq/log_entry_storage.hpp>

#include <gtest/gtest.h>

const char* TEST_FILE_NAME = "test_file_log_entry.data";
const char* TEST_FILE_NAME_META = "test_log_metadata.data";

using flowmq::LogEntryStorage;
using flowmq::MetadataStorage;
using flowmq::LogEntry;
using flowmq::LogEntryMetaData;

TEST(LogEntryStorage, TestWriteAndRead1000){
    //construct 10000 log entries

    std::vector<LogEntry> entries;
    for(int i = 0; i < 1000; ++i){
        LogEntry entry;
        entry.set_index(i);
        entry.set_term(2);
        entry.set_message_id(1000+i);
        entry.set_operation((i%2 == 0? LogEntry::ADD : LogEntry::COMMIT));
        entry.set_message(std::to_string(i)+ "_message");
        entries.push_back(std::move(entry));
    }

    EXPECT_EQ(0, LogEntryStorage::save_log_entry_to_file(TEST_FILE_NAME, entries));

    decltype(entries) loaded_entries;

    LogEntryStorage::load_log_entry_from_file(TEST_FILE_NAME, &loaded_entries);

    EXPECT_EQ(loaded_entries.size(), entries.size());

    for(int i = 0; i < 1000; ++i){
        EXPECT_TRUE(entries[i].SerializeAsString() == loaded_entries[i].SerializeAsString());
    }

    EXPECT_EQ(0, LogEntryStorage::append_log_entry_to_file(TEST_FILE_NAME, entries));
    decltype(entries) loaded_entries2;
    LogEntryStorage::load_log_entry_from_file(TEST_FILE_NAME, &loaded_entries2);
    EXPECT_EQ(loaded_entries2.size(), entries.size() * 2);
}

TEST(LogEntryStorage, TestWriteAndReadMetadata){

    LogEntryMetaData metadata{5678};
    EXPECT_EQ(0, MetadataStorage::save_metadata_to_file(TEST_FILE_NAME_META, metadata));
    decltype(metadata) loaded_metadata;

    MetadataStorage::load_metadata_from_file(TEST_FILE_NAME_META, &loaded_metadata);
    EXPECT_TRUE(metadata == loaded_metadata);
}

