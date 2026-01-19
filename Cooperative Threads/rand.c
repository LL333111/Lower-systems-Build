/*
 * rand.c
 *
 * Implementation of a random scheduler (schedules runnable threads randomly)
 */

#include "ut369.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>

static struct thread ** ready_queue = NULL;
static int count = 0;

int 
rand_init(void)
{
    ready_queue = malloc(sizeof(struct thread *)*THREAD_MAX_THREADS);
    count = 0;
    
    if (ready_queue != NULL) {
        return 0;
    }
    else {
        return THREAD_NOMEMORY;
    }
}

int
rand_enqueue(struct thread * thread)
{
    if (count >= THREAD_MAX_THREADS) {
        return THREAD_NOMORE;
    }

    ready_queue[count++] = thread;
    return 0;
}

struct thread *
rand_dequeue(void)
{
    struct thread * ret;
    int i;

    if (count <= 0) {
        return NULL;
    }

    i = rand() % count;
    ret = ready_queue[i];
    
    // override rq[i] with last element
    ready_queue[i] = ready_queue[--count];

    return ret;
}

struct thread *
rand_remove(Tid tid)
{
    struct thread * ret = NULL;
    int i;

    for (i = 0; i < count; i++) {
        if (ready_queue[i]->id == tid) {
            ret = ready_queue[i];
            break;
        }
    }

    // override rq[i] with last element (if found)
    if (ret != NULL) {  
        assert(count > 0);
        ready_queue[i] = ready_queue[--count];
    }

    return ret;
}

void
rand_destroy(void)
{
    free(ready_queue);
    ready_queue = NULL;
}