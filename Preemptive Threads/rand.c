/*
 * rand.c
 *
 * Implementation of a random scheduler (schedules runnable threads randomly)
 */

#include "ut369.h"
#include "thread.h"
#include "schedule.h"
#include "interrupt.h"
#include <stdlib.h>
#include <assert.h>

static struct thread ** prio_queue = NULL;
static int count = 0;

int 
rand_init(void)
{
    prio_queue = malloc(sizeof(struct thread *)*THREAD_MAX_THREADS);
    count = 0;
    
    if (prio_queue != NULL) {
        return 0;
    }
    else {
        return THREAD_NOMEMORY;
    }
}

int
rand_enqueue(struct thread * thread)
{
    assert(!interrupt_enabled());
    
    if (count >= THREAD_MAX_THREADS) {
        return THREAD_NOMORE;
    }

    prio_queue[count++] = thread;
    return 0;
}

struct thread *
rand_dequeue(void)
{
    struct thread * ret;
    int i;

    assert(!interrupt_enabled());
    if (count <= 0) {
        return NULL;
    }

    i = rand() % count;
    ret = prio_queue[i];
    
    // override rq[i] with last element
    prio_queue[i] = prio_queue[--count];

    return ret;
}

struct thread *
rand_remove(Tid tid)
{
    struct thread * ret = NULL;
    int i;

    assert(!interrupt_enabled());
    for (i = 0; i < count; i++) {
        if (prio_queue[i]->id == tid) {
            ret = prio_queue[i];
            break;
        }
    }

    // override rq[i] with last element (if found)
    if (ret != NULL) {  
        assert(count > 0);
        prio_queue[i] = prio_queue[--count];
    }

    return ret;
}

void
rand_destroy(void)
{
    free(prio_queue);
    prio_queue = NULL;
}