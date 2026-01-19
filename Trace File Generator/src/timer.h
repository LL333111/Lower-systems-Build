#ifndef __TIMER_H__
#define __TIMER_H__

#include <sys/time.h>


// Returns current time in seconds since the Epoch as a floating point number
static inline double get_time()
{
   struct timeval t;
   gettimeofday(&t, NULL);
   return t.tv_sec + t.tv_usec / 1000000.0;
}


#endif /* __TIMER_H__ */
