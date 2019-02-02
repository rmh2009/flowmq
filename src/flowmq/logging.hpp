#pragma once

#include <string>
#include <iostream>

namespace flowmq{

// A simple macro based logging module using 
// the stream style logging.

#ifdef FLOWMQ_LOG_STDOUT

#define LOG_INFO  \
    if(logging_info_disabled__){} else \
    std::cout << "INFO: "

#define LOG_ERROR \
    if(logging_error_disabled__){} else \
    std::cout << "ERROR: " 

#else

#define LOG_INFO  \
    if(true) {} else \
    std::cout << "INFO: "


#define LOG_ERROR \
    if(true) {} else \
    std::cout << "ERROR: " 

#endif

}
