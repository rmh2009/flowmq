# flowmq

## Into 

This is a distributed message queue in C++ based on the Raft consensus algorithm. 
The implementation uses boost Asio library for network communication and asyn executions.

C++11 features are used extensively such as lambda functions, R-value reference etc. 

At present state this is mostly my hobby project to learn about network programming and 
distibuted systems by implementing all the components required to build a persistent 
and resilient message queue system. These include socket communication layer, async 
execution management, cluster management, Raft algorithm protocals, serialization and 
storage modules for messages, support for message subscription and commits, 
support for arbitrary number of topics(to be implemented), etc. 



