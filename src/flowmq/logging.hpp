#pragma once

#include <string>
#include <iostream>

namespace flowmq{

// A simple macro based logging module using 
// the stream style logging.

#ifdef FLOWMQ_LOG_FULL
const bool logging_info_disabled__ = false;
const bool logging_error_disabled__ = false;
#else 
#ifdef FLOWMQ_LOG_NONE
const bool logging_info_disabled__ = true;
const bool logging_error_disabled__ = true;
#else  //default output error only
const bool logging_info_disabled__ = true;
const bool logging_error_disabled__ = false;
#endif
#endif

#define LOG_INFO  \
if(flowmq::logging_info_disabled__){} else \
std::cout << __FILE__ << ' ' << __LINE__ << " INFO: "

#define LOG_ERROR \
if(flowmq::logging_error_disabled__){} else \
std::cerr << __FILE__ << ' ' << __LINE__ << "ERROR: " 


}
