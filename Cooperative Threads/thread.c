/*
 * thread.c
 *
 * Implementation of the threading library.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include "interrupt.h"
#include <ucontext.h>
#include <sys/ucontext.h>
#include <stdint.h>

static struct thread *current;
static bool tid_used[THREAD_MAX_THREADS];

static struct thread *kernel_thread = NULL;

/* List of threads */
static struct thread *thread_list[THREAD_MAX_THREADS];
static void *stack_to_free = NULL;


/**************************************************************************
 * Cooperative threads: Refer to ut369.h and this file for the detailed 
 *                      descriptions of the functions you need to implement. 
 **************************************************************************/

/* Initialize the thread subsystem */
void
thread_init(void)
{
	static struct thread first_thread;

	// First create the tid list from 1 to THREAD_MAX_THREADS - 1
	// Do not inlucde Thread 0,becuase that is the kernal thread
	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
        tid_used[i] = false;
    }
    tid_used[0] = true;
	// Initialize the first thread, i.e., the kernel thread
	// The kernel thread always has tid 0
	first_thread.id = 0;
	first_thread.state = RUNNING;
	first_thread.in_or_not = 0;
	first_thread.next = NULL;
	first_thread.prev = NULL;
	first_thread.stack_base = NULL;
	first_thread.stack_size = 0;
	getcontext(&(first_thread.context));
	first_thread.exit_code = 0;
	first_thread.start_fn = NULL;
	first_thread.parg = NULL;
	first_thread.killed = false;
	current = &first_thread;
	thread_list[0] = &first_thread;
	current->resumed = false;
	kernel_thread = &first_thread;
}

/* Returns the tid of the current running thread. */
Tid
thread_id(void)
{
	return current->id;
}

/* Return the thread structure of the thread with identifier tid, or NULL if 
 * does not exist. Used by thread_yield and thread_wait's placeholder 
 * implementation.
 */
static struct thread * 
thread_get(Tid tid)
{	
	if (tid >= 0 && tid < THREAD_MAX_THREADS) {
		return thread_list[tid];
	}
	return NULL;
}

/* Return whether the thread with identifier tid is runnable.
 * Used by thread_yield and thread_wait's placeholder implementation
 */
static bool
thread_runnable(Tid tid)
{
	// It should be either at the running or ready state to be runnable
	if (!thread_get(tid)) {
		return false;
	}
	return (thread_get(tid)->state == READY || thread_get(tid)->state == RUNNING);
}

/* Context switch to the next thread. Used by thread_yield. */
static void
thread_switch(struct thread * next)
{
	getcontext(&current->context);
	 if (current->resumed) {
        current->resumed = false;  
		if (stack_to_free) {
			free(stack_to_free);
			stack_to_free = NULL;
		}
        return;
    }

    current->resumed = true;
    if (current->state == RUNNING) {
        current->state = READY;
        scheduler->enqueue(current);
    }
    next->state = RUNNING;
    current = next;
    if (next->killed) {
        thread_exit(THREAD_KILLED);
    }

    setcontext(&next->context);
}

/* Voluntarily pauses the execution of current thread and invokes scheduler
 * to switch to another thread.
 */
Tid
thread_yield(Tid want_tid)
{
	struct thread *next_thread;
	// If the current thread wants to yield to itself, just return its tid
	if (want_tid == thread_id()) {
		return thread_id();
	}
	// If the current thread wants to yield to any other thread
	// Dequeue the next thread from the scheduler's ready queue
	if (want_tid == THREAD_ANY) {
		next_thread = scheduler->dequeue();
		// If there is no other runnable thread, return THREAD_NONE
		if (next_thread == NULL) {
			return THREAD_NONE;
		}
	}
	// If the current thread wants to yield to a specific thread
	else {
		// Check if the specified thread exists
		if (thread_get(want_tid) == NULL) {
			return THREAD_INVALID;
		}
		// Check if the specified thread is in the READY state
		if (thread_get(want_tid)->state != READY) {
			return THREAD_INVALID;
		}
		next_thread = scheduler->remove(want_tid);
		// If the specified thread does not exist or is not runnable, return THREAD_INVALID
		if (next_thread == NULL) {
			return THREAD_INVALID;
		}
	}
	Tid ret = next_thread->id;
	thread_switch(next_thread);
	return ret;
}

/* Fully clean up a thread structure and make its tid available for reuse.
 * Used by thread_wait's placeholder implementation
 */
static void
thread_destroy(struct thread * dead)
{
	scheduler->remove(dead->id);

    if (dead->stack_base) {
        free(dead->stack_base);
        dead->stack_base = NULL;
        dead->stack_size = 0;
    }

    thread_list[dead->id] = NULL;

 
    if (dead->id >= 0 && dead->id < THREAD_MAX_THREADS) {
        tid_used[dead->id] = false;
    }


    if (dead != kernel_thread) {
        free(dead);
    }
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
static void
thread_stub(int (*thread_main)(void *), void *arg)
{
	if (stack_to_free) {
        free(stack_to_free);
        stack_to_free = NULL;
    }
	int ret = thread_main(arg); // call thread_main() function with arg
	thread_exit(ret);
}

Tid
thread_create(int (*fn)(void *), void *parg)
{
    struct thread *new_thread = malloc(sizeof(struct thread));
    if (new_thread == NULL) {                          
        return THREAD_NOMEMORY;
    }

    int tid = -1;
    for (int i = THREAD_MAX_THREADS - 1; i >= 0; --i) {
        if (!tid_used[i]) {
            tid_used[i] = true;
            tid = i;
            break;
        }
    }
    if (tid < 0) {                
        free(new_thread);
        return THREAD_NOMORE;
    }


    new_thread->id = tid;                             
    new_thread->state = READY;
    new_thread->in_or_not = 0;
    new_thread->next = NULL;
    new_thread->prev = NULL;
    new_thread->stack_size = THREAD_MIN_STACK;
    new_thread->exit_code = 0;
    new_thread->start_fn = fn;
    new_thread->parg = parg;
    new_thread->killed = false;
    new_thread->resumed = false;

    new_thread->stack_base = malloc(THREAD_MIN_STACK);
    if (!new_thread->stack_base) {
        free(new_thread);
        return THREAD_NOMEMORY;
    }
    getcontext(&(new_thread->context));

    uintptr_t top = (uintptr_t)new_thread->stack_base + THREAD_MIN_STACK;
    top &= ~(uintptr_t)0xF;
    top -= 8;
   
    new_thread->context.uc_stack.ss_sp    = new_thread->stack_base;
    new_thread->context.uc_stack.ss_size  = new_thread->stack_size;
    new_thread->context.uc_stack.ss_flags = 0;
    new_thread->context.uc_link           = NULL;

    new_thread->context.uc_mcontext.gregs[REG_RIP] = (greg_t)thread_stub;
    new_thread->context.uc_mcontext.gregs[REG_RSP] = (greg_t)top;
    new_thread->context.uc_mcontext.gregs[REG_RDI] = (greg_t)fn;
    new_thread->context.uc_mcontext.gregs[REG_RSI] = (greg_t)parg;

    thread_list[new_thread->id] = new_thread;
    scheduler->enqueue(new_thread);

    return new_thread->id;
}

Tid
thread_kill(Tid tid)
{
	if (tid == thread_id()) {
		return THREAD_INVALID;
	}

	struct thread * target = thread_get(tid);
	if (target == NULL) {
		return THREAD_INVALID;
	}
	if (target->state == ZOMBIE) {
        return tid;
    }

	target->killed = true;
	return tid;
}

void
thread_exit(int exit_code)
{
	struct thread *next_thread = scheduler->dequeue();
	if (next_thread == NULL) {
		thread_end();
		ut369_exit(exit_code);
	}
	current->exit_code = exit_code;
	current->state = ZOMBIE;
	if (current->id != 0 && current->stack_base) {
    	stack_to_free = current->stack_base;   
    	current->stack_base = NULL;            
    	current->stack_size = 0;
	} 
	thread_switch(next_thread);
}

/* Clean-up logic to unload the threading system. Used by ut369.c. You may 
 * assume all threads are either freed or in the zombie state when this is 
 * called.
 */
void
thread_end(void)
{
	
    for (int i = 1; i < THREAD_MAX_THREADS; i++) {
        struct thread *thread = thread_list[i];
        if (!thread) {
			continue;
		}
        if (thread->stack_base) {
            free(thread->stack_base);
            thread->stack_base = NULL;
        }
        free(thread);
        thread_list[i] = NULL;
    }
}

/**************************************************************************
 * Preemptive threads: Refer to ut369.h for the detailed descriptions of 
 *                     the functions you need to implement. 
 **************************************************************************/

Tid
thread_wait(Tid tid, int *exit_code)
{
	// This is a placeholder implementation for cooperative threads.
	// It will not work once you start preemptive threads. Do not change
	// this for A1, but definitely change it for A2.
	
	if (tid == thread_id()) {
		return THREAD_INVALID;
	}

	// If thread does not exist, return error
	struct thread * target = thread_get(tid);
	if (target == NULL) {
		return THREAD_INVALID;
	}

	// Continue to yield to the thread until its no longer runnable
	while (thread_runnable(tid)) {
		int ret = thread_yield(tid);
		assert(ret == tid);
	}

	// Clean up all resources used by this thread, and make its tid available
	thread_destroy(target);

	// Unused for now
	(void)exit_code;
	return 0;
}

Tid
thread_sleep(fifo_queue_t *queue)
{
	/* TBD */
	(void)queue;
	return THREAD_TODO;
}

/* When the 'all' parameter is 1, wake up all threads waiting in the queue.
 * returns whether a thread was woken up on not. 
 */
int
thread_wakeup(fifo_queue_t *queue, int all)
{
	/* TBD */
	(void)queue;
	(void)all;
	return THREAD_TODO;
}

struct lock {
	/* ... fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock = malloc(sizeof(struct lock));
	/* TBD */
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);
	/* TBD */
	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);
	/* TBD */
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);
	/* TBD */
}

struct cv {
	/* ... fill this in ... */
};

struct cv *
cv_create(void)
{
	struct cv *cv = malloc(sizeof(struct cv));
	/* TBD */
	return cv;
}

void 
cv_destroy(struct cv *cv)
{
	assert(cv);
	/* TBD */
	free(cv);
}

void 
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
	/* TBD */
}

void cv_signal(struct cv *cv)
{
	assert(cv != NULL);
	/* TBD */
}

void cv_broadcast(struct cv *cv)
{
	assert(cv != NULL);
	/* TBD */
}
