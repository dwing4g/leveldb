#pragma once

#include <mutex>
#include <condition_variable>

typedef std::mutex pthread_mutex_t;
typedef std::condition_variable_any pthread_cond_t;
typedef std::once_flag* pthread_once_t;

#define PTHREAD_ONCE_INIT new std::once_flag
