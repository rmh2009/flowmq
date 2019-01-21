set(BOOST_LIB /Users/hong/usr/boost-1.68.0/lib/libboost_system.a)
set(PROTOBUF_LIB /Users/hong/usr/lib/libprotobuf.a)

set (NODE_SRCS app_cluster_node.cpp)
add_executable(flowmq_node ${NODE_SRCS})
target_link_libraries(flowmq_node libflowmq
    ${PROTOBUF_LIB}
    ${BOOST_LIB})

set (TEST_CLIENT_SRCS test_client.cpp)
add_executable(flowmq_test_client ${TEST_CLIENT_SRCS})
target_link_libraries(flowmq_test_client 
        ${BOOST_LIB}
        ${PROTOBUF_LIB}
        libflowmq
        )

set (APP_CLIENT app_mq_client.cpp)
add_executable(app_mq_client ${APP_CLIENT})
target_link_libraries(app_mq_client
        ${BOOST_LIB}
        ${PROTOBUF_LIB}
        libflowmq
        )

install(TARGETS flowmq_node flowmq_test_client app_mq_client DESTINATION ${PROJECT_SOURCE_DIR}/build)
