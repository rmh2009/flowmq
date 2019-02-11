set (NODE_SRCS app_cluster_node.cpp)
add_executable(flowmq_node ${NODE_SRCS})
target_link_libraries(flowmq_node 
    libflowmq
    ${Protobuf_LIBRARY}
    ${Boost_LIBRARIES}
    )

set (TEST_CLIENT_SRCS test_client.cpp)
add_executable(flowmq_integration_test ${TEST_CLIENT_SRCS})
target_link_libraries(flowmq_integration_test 
    ${Protobuf_LIBRARY}
    ${Boost_LIBRARIES}
    libflowmqclient
    )

set (FLOWMQ_CLIENT app_mq_client.cpp)
add_executable(flowmq_client ${FLOWMQ_CLIENT})
target_link_libraries(flowmq_client
    ${Protobuf_LIBRARY}
    ${Boost_LIBRARIES}
    libflowmq
    )

install(TARGETS flowmq_node flowmq_integration_test flowmq_client DESTINATION ${PROJECT_SOURCE_DIR}/build)
