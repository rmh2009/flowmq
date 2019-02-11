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

## BUILD 

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

### Build

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

### Compiler Flags
Use compiler flag "FLOWMQ_LOG_FULL" to enable full logging, use 
flag "FLOWMQ_LOG_NONE" to suppress all internal logging. If neither 
is set the default behavior would only log error messages.

### Test 

run unit tests
```
make test
```

## Tutorial

### Start servers 

build/node.config is an exampe config file that can be used to start 
a local cluster. By default this specifies 5 cluster nodes.

Each node specifies a inter cluster port and a client facing port. 
Client can only connect to the client facing port to send and receive 
messages. The simple client API provided below handles the redirection 
to the correct leader.

This config file also specifies several partitions to be started in this cluster. 
Partitions are completely separated, each partition has its own leader. 
By default each partition also runs on its own thread, it can also be 
easily changed so that one thread can handle multiple partitions.

```
# start node 0
build/flowmq_node node.config 0
# start node 1
build/flowmq_node node.config 0
# start other nodes ...
```

Upon start the cluster nodes will start voting and have a leader selected 
for each partition. Leader will start syncing data to other followers, 
    and is also responsible for communicating with client.

### Client API

A simple client API is provided for interacting with the queue cluster. 
(You can also reference the integration test *application/test_client.cpp* for example uses)

Start a client.
```
#include <flowmq_client/simple_client.hpp>

tcp::resolver resolver(io_context);
auto endpoints = resolver.resolve("localhost", "9001"); 
int partition_id = 0; // this is configured in the config file
flowmq::SimpleClient client(partition_id, io_context, endpoints);

// start the io_context to run 
std::thread t([&io_context](){  
        io_context.run(); 
        });
```

Register handler for receiving messages.
```
// messages are consumed asyncrhonously, so register a handler first before 
// opening a queue.
std::vector<int> message_ids; 
client.register_handler([&message_ids](std::string msg, int message_id){
        LOG_INFO << "Got message " << msg;
        message_ids.push_back(message_id);
        });

// open a queue (queue name and mode are not supported yet, this will just open the partition_id)
// This is a blocking call, will redirect to the correct leader.
client.open_queue_sync("test_queue", 0);
```

Send message, this is a non-blocking API.
```
client.send_message(std::string("test") + std::to_string(i));
```

Commit a consumed message, this also non-blocking. A committed message will not
be delivered to any other clients. If a message is not committed by current client, 
when current client disconnects, it will be redelivered to other clients 
for consumption. 
```
// consume all received messages
for(auto id : message_ids){
    client.commit_message(id);
}

```

### Integration Test 

A integration test file is available at application/test_client.cpp. 
It is installed to the build directory after the 'make install' command:
```
build/flowmq_integration_test localhost 9003 0
```
Where 9003 is the client port of any cluster node, 0 specifies the partition to test.

## Design Doc and Future Plan

TBD


