# Flowmq 

## Introduction 

Flowmq is a lightweight, distributed, and resilient message queue implemented in C++ 
based on the Raft consensus algorithm. Key features include resilience to node crashes, 
data persistence on disk, round-robin message delivery, message consuption, etc.

At present state this is mainly my hobby project to learn about network programming and 
distibuted systems by implementing the major components and algorithms to build a distributed
message queue system. The key components I'm interested in through this exercise include socket 
communication layer, async executions paradigm, cluster management, Raft algorithm protocals, 
serialization and storage, "zero"-copy optimizations, etc.

Key features supported at present stage:
- Resilient server nodes. Handles node crashes, leader 
  re-election, data recovery automatically.
- Messages are persisted on disk.
- Support for message delivery to multiple consumers based on round-robin.
- Support message consumption from consumer. 
- Strong consensus guarantee based on Raft Algorithm.

More features to be implemented:
- Support for multiple message queues.
- Support for partitioning within a message queue.
- Improve multithread performance
- Benchmark tools

The implementation uses boost Asio library for network communication and async executions. 
This library also depends on Protobuf. C++11 features are used extensively such as 
lambda functions, R-value reference etc. 

External library dependencies: 
- Boost Asio (1.68 or above recommended)
- Protobuf
- Google Test 

## Install Dependencies

On mac, boost and protobuf can be installed using brew 

```
brew install protobuf --c++11
brew install boost --c++11
```

On other platforms (or if the brew install failed), 
you can try build and install the libraries from github repo 

https://github.com/boostorg/boost

https://github.com/protocolbuffers/protobuf

Google test is not available in brew, so you can download 
and build it using CMake:

https://github.com/google/googletest

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

## Test 

run unit tests
```
cmake test
```


