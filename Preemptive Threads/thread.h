/*
 * thread.h
 *
 * Definition of the thread structure and internal helper functions.
 *
 * You may add more declarations/definitions in this file.
 */

#ifndef _THREAD_H_
#define _THREAD_H_

#include "ut369.h"
#include <stdbool.h>
#include <ucontext.h>

struct thread
{
    Tid id;

    struct thread *next;
    struct thread *prev;
    int in_or_not;
    enum thread_state
    {
        RUNNING,
        READY,
        SLEEPING,
        ZOMBIE
    } state;

    void *stack_base;
    size_t stack_size;
    ucontext_t context;
    int exit_code;
    int (*start_fn)(void *);
    void *parg;
    bool killed;
    Tid yield_tid;
    fifo_queue_t *wait_queue;
    bool resumed;
    fifo_queue_t *join_queue;
    bool reaped;
    bool w_exit;
};

// functions defined in thread.c
void thread_init(void);
void thread_end(void);

// functions defined in ut369.c
void ut369_exit(int exit_code);

/**************************************************************************
 * (A2) API functions for preemptive threads only
 **************************************************************************/

/* Suspend the execution of the calling thread and schedules another thread
 * to run. The calling thread is placed in the specified wait queue and will
 * not resume execution until it is later scheduled.
 *
 * Parameters:
 * - queue: A pointer to the FIFO queue where the calling thread will be placed.
 *          This queue must be valid and properly initialized.
 *
 * Behavior:
 * - This function should **crash** on an assertion error if interrupt is not
 *   disabled prior to invocation, i.e., the caller is expected to disable
 *   interrupt before calling this function.
 *
 * Return Values:
 * - On success: Returns the identifier of the thread that was switched to
 *   when the calling thread was put to sleep. This function will not return
 *   to the calling thread until it is scheduled to run again.
 * - On failure: The calling thread continues running, and one of the following
 *   error codes is returned:
 *   - THREAD_INVALID: The specified queue is invalid (e.g., it is NULL).
 *   - THREAD_DEADLOCK: a deadlock would occur if the current thread went to
 *     sleep on the queue.
 *   - THREAD_NONE: No other threads, aside from the caller, are available
 *     to run. If both THREAD_DEADLOCK and THREAD_NONE conditions hold,
 *     THREAD_DEADLOCK is returned.
 */
Tid thread_sleep(fifo_queue_t *queue);

/* Wake up one or more threads that are suspended in the specified wait queue
 * and move them to the ready queue.
 *
 * Behavior:
 * - Threads are woken up in FIFO order, ensuring that the first thread to
 *   sleep is the first to be woken.
 * - This function should **crash** on an assertion error if interrupt is not
 *   disabled prior to invocation, i.e., the caller is expected to disable
 *   interrupt before calling this function.
 *
 * Parameters:
 * - queue: A pointer to the FIFO queue from which threads are to be woken.
 * - all: Determines how many threads to wake up:
 *        - 0: Wake up a single thread.
 *        - 1: Wake up all suspended threads.
 *
 * Return Value:
 * - Returns the number of threads that were woken up
 * - If the wait queue is empty, returns 0.
 */
int thread_wakeup(fifo_queue_t *queue, int all);

#endif /* _THREAD_H_ */
