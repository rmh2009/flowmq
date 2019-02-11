add_library(libflowmqclient
    simple_client.cpp
    )

target_link_libraries(libflowmqclient libflowmq)
target_include_directories(libflowmqclient PUBLIC ..)

