/*
 * fcfs.c
 *
 * Implementation of a first-come first-served scheduler.
 * Becomes round-robin once preemption is enabled.
 */

#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>

static struct thread **ready_queue = NULL;
static int count = 0;
static int start = 0;
static int end = 0;

int fcfs_init(void)
{
    ready_queue = malloc(sizeof(struct thread *) * THREAD_MAX_THREADS);
    count = 0;

    if (ready_queue != NULL)
    {
        count = 0;
        start = 0;
        end = 0;
        return 0;
    }
    else
    {
        return THREAD_NOMEMORY;
    }
}

int fcfs_enqueue(struct thread *thread)
{
    if (count >= THREAD_MAX_THREADS)
    {
        return THREAD_NOMORE;
    }
    if (start >= THREAD_MAX_THREADS)
    {
        start = 0;
    }

    ready_queue[start++] = thread;
    count++;
    thread->in_or_not = 1;

    return 0;
}

struct thread *
fcfs_dequeue(void)
{
    if (count <= 0)
    {
        return NULL;
    }
    if (end >= THREAD_MAX_THREADS)
    {
        end = 0;
    }
    count--;
    struct thread *ret = ready_queue[end++];
    ret->in_or_not = 0;
    return ret;
}

struct thread *
fcfs_remove(Tid tid)
{
    struct thread *ret = NULL;
    int i;
    int search_position = end;

    if (count <= 0)
    {
        return NULL;
    }

    for (i = 0; i < count; i++)
    {
        if (search_position >= THREAD_MAX_THREADS)
        {
            search_position = 0;
        }
        if (ready_queue[search_position]->id == tid)
        {
            ret = ready_queue[search_position];
            break;
        }
        search_position += 1;
    }

    if (ret == NULL)
    {
        return NULL;
    }

    if (search_position == end)
    {
        if (end >= THREAD_MAX_THREADS - 1)
        {
            end = 0;
        }
        else
        {
            end += 1;
        }
        ret->in_or_not = 0;
        count--;
        return ret;
    }

    int tempo_start = start - 1;
    if (tempo_start < 0)
    {
        tempo_start = THREAD_MAX_THREADS - 1;
    }

    while (search_position != tempo_start)
    {
        int next = search_position + 1;
        if (next >= THREAD_MAX_THREADS)
        {
            next = 0;
        }
        ready_queue[search_position] = ready_queue[next];
        search_position = next;
    }

    if (start <= 0)
    {
        start = THREAD_MAX_THREADS - 1;
    }
    else
    {
        start -= 1;
    }

    count--;
    ret->in_or_not = 0;
    return ret;
}

void fcfs_destroy(void)
{
    free(ready_queue);
    ready_queue = NULL;
    count = 0;
    start = 0;
    end = 0;
}