cmake_minimum_required (VERSION 3.0)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wconversion -Wno-sign-conversion")
project (Flowmq)

# If Boost and Protobuf are installed in nonstandard 
# locations, use environment variable CMAKE_PREFIX_PATH to 
# specify root paths of Boost and Protobuf libraries
# so that find_package can locate them.
# e.g. 
# CMAKE_PREFIX_PATH=/users/yourname/usr cmake ..

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.68 REQUIRED COMPONENTS system)

set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf REQUIRED)
find_package(GTest REQUIRED)

message(${Boost_INCLUDE_DIRS})
message(${Boost_LIBRARY_DIRS})
message(${Boost_LIBRARIES})
message(${Protobuf_INCLUDE_DIRS})
message(${Protobuf_LIBRARY})
message(${GTEST_LIBRARIES})
message(${GTEST_MAIN_LIBRARIES})

SET(CUSTOM_INCLUDE_DIRS 
    ${Boost_INCLUDE_DIRS} 
    ${Protobuf_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
    )

INCLUDE_DIRECTORIES(SYSTEM ${CUSTOM_INCLUDE_DIRS})

enable_testing()

#add_definitions(-DFLOWMQ_LOG_LEVEL_1)
add_subdirectory(src)
