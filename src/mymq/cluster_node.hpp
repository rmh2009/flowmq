#pragma once 

#include <iostream>
#include <map>
#include <functional>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <mymq/message.hpp>
#include <mymq/session.hpp>

using boost::asio::ip::tcp;

// Player in a distributed system using 
// Raft consensus algorithm for leader election 
// and log replication. 
//
// Internally uses ClusterManager to talk to 
// other nodes in the cluster.
//
// Uses other in-memory and on-disk 
// log manangement classes.
//
class ClusterNode{



};


