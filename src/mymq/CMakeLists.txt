
set(PROTOBUF_LIB /Users/hong/usr/lib/libprotobuf.a)

add_library(libflowmq 
    flow_message.pb.cc
    client_manager.cpp
    cluster_manager.cpp
    log_entry_storage.cpp
    message_queue.cpp
    session.cpp
    cluster_node.cpp
    generic_client.cpp
    )

add_executable(log_entry_storage_test
    log_entry_storage_test.cpp)
target_link_libraries(log_entry_storage_test 
    ${PROTOBUF_LIB}
    gtest_main
    libflowmq)

add_test(NAME EntryStorageTest COMMAND log_entry_storage_test)
