#ifndef _TEST_H_

#include "../ut369.h"
#include "../queue.h"
#include "../thread.h"
#include "../interrupt.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

static inline int
thread_ret_ok(Tid ret)
{
	return (ret >= 0 ? 1 : 0);
}

#endif /* _TEST_H_ */