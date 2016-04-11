#pragma once

#include <pthread.h>
#include <time.h>

int pthread_mutex_timedlock(pthread_mutex_t* mutex, const timespec* abs_timeout); 
