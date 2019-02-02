add_library(libflowmq 
    flow_message.pb.cc
    client_manager.cpp
    cluster_manager.cpp
    log_entry_storage.cpp
    message_queue.cpp
    session.cpp
    cluster_node.cpp
    generic_client.cpp
    configuration.cpp
    )
target_include_directories(libflowmq PUBLIC ..)

add_executable(log_entry_storage_test
    log_entry_storage_test.cpp)
target_link_libraries(log_entry_storage_test 
    ${Protobuf_LIBRARY}
    ${GTEST_BOTH_LIBRARIES}
    libflowmq)

add_executable(configuration_test
    configuration_test.cpp)
target_link_libraries(configuration_test 
    ${GTEST_BOTH_LIBRARIES}
    libflowmq)

add_test(NAME EntryStorageTest COMMAND log_entry_storage_test)
add_test(NAME ConfigurationTest COMMAND configuration_test)