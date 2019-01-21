cmake_minimum_required (VERSION 3.0)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
project (Flowmq)

SET(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/src"
    /Users/hong/usr/boost-1.68.0/include
    /Users/hong/usr/include
    )
INCLUDE_DIRECTORIES( ${INCLUDE_DIR} )
SET(LINK_DIR /Users/hong/usr/lib)
LINK_DIRECTORIES(${LINK_DIR})

enable_testing()
add_subdirectory(src)
add_subdirectory(third_party/googletest/googletest)
