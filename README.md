# Flowmq - A Lightweight Distributed Message Queue based on Raft

## Introduction 

This is a distributed message queue in C++ based on the Raft consensus algorithm. 
The implementation uses boost Asio library for network communication and async executions. 
This library also depends on Protobuf.

External library dependencies: 
- Boost Asio (1.68 or above recommended)
- Protobuf

C++11 features are used extensively such as lambda functions, R-value reference etc. 

At present state this is mostly my hobby project to learn about network programming and 
distibuted systems by implementing all the components required to build a persistent 
and resilient message queue system. These include socket communication layer, async 
execution management, cluster management, Raft algorithm protocals, serialization and 
storage modules for messages, support for message subscription and commits, 
support for arbitrary number of topics(to be implemented), etc. 

## Build

This repo uses CMAKE for building the library and test binaries. 
If Boost and Protobuf are installed in nonstandard 
locations, use environment variable CMAKE_PREFIX_PATH to 
specify root paths of Boost and Protobuf libraries
so that find_package can locate them.

```
mkdir cmake.build 
cd cmake.build
CMAKE_PREFIX_PATH=/path/to/your/lib/root cmake ..
make
```



