set(BOOST_LIB /Users/hong/usr/boost-1.68.0/lib/libboost_system.a)
set(PROTOBUF_LIB /Users/hong/usr/lib/libprotobuf.a)

set (NODE_SRCS app_cluster_node.cpp)
add_executable(flowmq_node ${NODE_SRCS})
target_link_libraries(flowmq_node libflowmq
    ${PROTOBUF_LIB}
    ${BOOST_LIB})

set (TEST_CLIENT_SRCS test_client.cpp)
add_executable(flowmq_integration_test ${TEST_CLIENT_SRCS})
target_link_libraries(flowmq_integration_test 
    ${BOOST_LIB}
    ${PROTOBUF_LIB}
    libflowmq
    )

set (FLOWMQ_CLIENT app_mq_client.cpp)
add_executable(flowmq_client ${FLOWMQ_CLIENT})
target_link_libraries(flowmq_client
    ${BOOST_LIB}
    ${PROTOBUF_LIB}
    libflowmq
    )

install(TARGETS flowmq_node flowmq_integration_test flowmq_client DESTINATION ${PROJECT_SOURCE_DIR}/build)
