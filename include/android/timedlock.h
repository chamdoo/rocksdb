#ifndef __MUTEX_TIMELOCK
#define __MUTEX_TIMELOCK

#include <pthread.h>
#include <time.h>
#include <errno.h>

int pthread_mutex_timedlock(pthread_mutex_t* mutex, const timespec* abs_timeout) {
	// get current abs time
	timespec cur_time;

	clock_gettime(CLOCK_MONOTONIC, &cur_time);
	
	// if time has already passed
	if (cur_time.tv_sec > abs_timeout->tv_sec) {
		return ETIMEDOUT;
	} else if (cur_time.tv_sec == abs_timeout->tv_sec && cur_time.tv_nsec > abs_timeout->tv_nsec) {
		return ETIMEDOUT;
	}

	unsigned msecs = (abs_timeout->tv_sec - cur_time.tv_sec)*1000 + (abs_timeout->tv_nsec - cur_time.tv_nsec)/1000000;
	return pthread_mutex_lock_timeout_np(mutex, msecs);
}

#endif
