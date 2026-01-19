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
void thread_init(void)
{
	static struct thread first_thread;

	// First create the tid list from 1 to THREAD_MAX_THREADS - 1
	// Do not inlucde Thread 0,becuase that is the kernal thread
	for (int i = 0; i < THREAD_MAX_THREADS; i++)
	{
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
	first_thread.wait_queue = NULL;
	first_thread.join_queue = NULL;
	first_thread.reaped = false;
	first_thread.w_exit = false;
	first_thread.yield_tid = 0;
}

/* Returns the tid of the current running thread. */
Tid thread_id(void)
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
	if (tid >= 0 && tid < THREAD_MAX_THREADS)
	{
		return thread_list[tid];
	}
	return NULL;
}

/* Return whether the thread with identifier tid is runnable.
 * Used by thread_yield and thread_wait's placeholder implementation
 */
__attribute__((unused)) static bool thread_runnable(Tid tid)
{
	// It should be either at the running or ready state to be runnable
	if (!thread_get(tid))
	{
		return false;
	}
	return (thread_get(tid)->state == READY || thread_get(tid)->state == RUNNING);
}

/* Context switch to the next thread. Used by thread_yield. */
static void
thread_switch(struct thread *next)
{
	getcontext(&current->context);
	if (current->resumed)
	{
		current->resumed = false;
		if (stack_to_free)
		{
			free(stack_to_free);
			stack_to_free = NULL;
		}
		return;
	}

	current->resumed = true;
	if (current->state == RUNNING)
	{
		current->state = READY;
		scheduler->enqueue(current);
	}
	next->state = RUNNING;
	current = next;
	if (current->killed)
	{
		thread_exit(THREAD_KILLED);
	}
	if (next->killed)
	{
		thread_exit(THREAD_KILLED);
	}
	setcontext(&next->context);
}

/* Voluntarily pauses the execution of current thread and invokes scheduler
 * to switch to another thread.
 */
Tid thread_yield(Tid want_tid)
{
	int previous_status = interrupt_set(0);
	if (current->killed)
	{
		thread_exit(THREAD_KILLED);
	}

	struct thread *next_thread;
	// If the current thread wants to yield to itself, just return its tid
	if (want_tid == thread_id())
	{
		interrupt_set(previous_status);
		return thread_id();
	}
	// If the current thread wants to yield to any other thread
	// Dequeue the next thread from the scheduler's ready queue
	if (want_tid == THREAD_ANY)
	{
		next_thread = scheduler->dequeue();
		// If there is no other runnable thread, return THREAD_NONE
		if (next_thread == NULL)
		{
			interrupt_set(previous_status);
			return THREAD_NONE;
		}
	}
	// If the current thread wants to yield to a specific thread
	else
	{
		// Check if the specified thread exists
		if (thread_get(want_tid) == NULL)
		{
			interrupt_set(previous_status);
			return THREAD_INVALID;
		}
		// Check if the specified thread is in the READY state
		if (thread_get(want_tid)->state != READY)
		{
			interrupt_set(previous_status);
			return THREAD_INVALID;
		}
		next_thread = scheduler->remove(want_tid);
		// If the specified thread does not exist or is not runnable, return THREAD_INVALID
		if (next_thread == NULL)
		{
			interrupt_set(previous_status);
			return THREAD_INVALID;
		}
	}
	Tid ret = next_thread->id;
	thread_switch(next_thread);

	interrupt_set(previous_status);
	return ret;
}

/* Fully clean up a thread structure and make its tid available for reuse.
 * Used by thread_wait's placeholder implementation
 */
static void
thread_destroy(struct thread *dead)
{
	int previous_status = interrupt_set(0);
	scheduler->remove(dead->id);

	if (dead->stack_base)
	{
		free(dead->stack_base);
		dead->stack_base = NULL;
		dead->stack_size = 0;
	}

	if (dead->join_queue)
	{
		queue_destroy(dead->join_queue);
		dead->join_queue = NULL;
	}

	thread_list[dead->id] = NULL;

	if (dead->id >= 0 && dead->id < THREAD_MAX_THREADS)
	{
		tid_used[dead->id] = false;
	}

	if (dead != kernel_thread)
	{
		free(dead);
	}
	interrupt_set(previous_status);
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function.
 */
static void
thread_stub(int (*thread_main)(void *), void *arg)
{
	if (stack_to_free)
	{
		free(stack_to_free);
		stack_to_free = NULL;
	}
	interrupt_on();
	if (current->killed)
	{
		thread_exit(THREAD_KILLED);
	}
	int ret = thread_main(arg); // call thread_main() function with arg
	thread_exit(ret);
}

Tid thread_create(int (*fn)(void *), void *parg)
{
	int previous_status = interrupt_set(0);
	struct thread *new_thread = malloc(sizeof(struct thread));
	if (new_thread == NULL)
	{
		interrupt_set(previous_status);
		return THREAD_NOMEMORY;
	}

	int tid = -1;
	for (int i = THREAD_MAX_THREADS - 1; i >= 0; --i)
	{
		if (!tid_used[i])
		{
			tid_used[i] = true;
			tid = i;
			break;
		}
	}
	if (tid < 0)
	{
		free(new_thread);
		interrupt_set(previous_status);
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
	new_thread->wait_queue = NULL;
	new_thread->join_queue = NULL;
	new_thread->reaped = false;
	new_thread->w_exit = false;
	new_thread->yield_tid = 0;

	new_thread->stack_base = malloc(THREAD_MIN_STACK);
	if (!new_thread->stack_base)
	{
		free(new_thread);
		interrupt_set(previous_status);
		return THREAD_NOMEMORY;
	}
	getcontext(&(new_thread->context));

	uintptr_t top = (uintptr_t)new_thread->stack_base + THREAD_MIN_STACK;
	top &= ~(uintptr_t)0xF;
	top -= 8;

	new_thread->context.uc_stack.ss_sp = new_thread->stack_base;
	new_thread->context.uc_stack.ss_size = new_thread->stack_size;
	new_thread->context.uc_stack.ss_flags = 0;
	new_thread->context.uc_link = NULL;

	new_thread->context.uc_mcontext.gregs[REG_RIP] = (greg_t)thread_stub;
	new_thread->context.uc_mcontext.gregs[REG_RSP] = (greg_t)top;
	new_thread->context.uc_mcontext.gregs[REG_RDI] = (greg_t)fn;
	new_thread->context.uc_mcontext.gregs[REG_RSI] = (greg_t)parg;

	thread_list[new_thread->id] = new_thread;
	scheduler->enqueue(new_thread);
	interrupt_set(previous_status);

	return new_thread->id;
}

Tid thread_kill(Tid tid)
{
	int previous_status = interrupt_set(0);
	if (tid == thread_id())
	{
		interrupt_set(previous_status);
		return THREAD_INVALID;
	}

	struct thread *target = thread_get(tid);
	if (target == NULL)
	{
		interrupt_set(previous_status);
		return THREAD_INVALID;
	}
	if (target->state == ZOMBIE)
	{
		interrupt_set(previous_status);
		return tid;
	}

	target->killed = true;

	if (target->state == SLEEPING)
	{
		if (target->wait_queue)
		{
			queue_remove(target->wait_queue, target->id);
			target->wait_queue = NULL;
		}
		target->state = READY;
		scheduler->enqueue(target);
	}
	interrupt_set(previous_status);
	return tid;
}

void thread_exit(int exit_code)
{
	int previous_status = interrupt_set(0);

	current->exit_code = exit_code;
	current->state = ZOMBIE;

	if (current->join_queue && queue_count(current->join_queue) > 0)
	{
		current->w_exit = true;
		thread_wakeup(current->join_queue, 1);
	}

	struct thread *next_thread = scheduler->dequeue();
	if (next_thread == NULL)
	{
		thread_end();
		interrupt_end();
		ut369_exit(exit_code);
	}

	if (current->id != 0 && current->stack_base)
	{
		stack_to_free = current->stack_base;
		current->stack_base = NULL;
		current->stack_size = 0;
	}
	thread_switch(next_thread);
	interrupt_set(previous_status);
}

/* Clean-up logic to unload the threading system. Used by ut369.c. You may
 * assume all threads are either freed or in the zombie state when this is
 * called.
 */
void thread_end(void)
{
	int previous_status = interrupt_set(0);
	for (int i = 1; i < THREAD_MAX_THREADS; i++)
	{
		struct thread *thread = thread_list[i];
		if (!thread)
		{
			continue;
		}
		if (thread->stack_base)
		{
			free(thread->stack_base);
			thread->stack_base = NULL;
		}
		free(thread);
		thread_list[i] = NULL;
	}
	interrupt_set(previous_status);
}

/**************************************************************************
 * Preemptive threads: Refer to ut369.h for the detailed descriptions of
 *                     the functions you need to implement.
 **************************************************************************/

Tid thread_wait(Tid tid, int *exit_code)
{
	int previous_status = interrupt_set(0);

	if (tid < 0 || tid >= THREAD_MAX_THREADS)
	{
		interrupt_set(previous_status);
		return THREAD_INVALID;
	}
	if (tid == thread_id())
	{
		interrupt_set(previous_status);
		return THREAD_INVALID;
	}

	struct thread *target = thread_get(tid);
	if (!target)
	{
		interrupt_set(previous_status);
		return THREAD_INVALID;
	}

	if (target->state == ZOMBIE)
	{
		if (target->reaped)
		{
			interrupt_set(previous_status);
			return THREAD_INVALID;
		}
		if (target->w_exit)
		{
			interrupt_set(previous_status);
			return THREAD_INVALID;
		}

		if (exit_code)
		{
			*exit_code = target->exit_code;
		}
		target->reaped = true;
		thread_destroy(target);
		interrupt_set(previous_status);
		return tid;
	}

	if (!target->join_queue)
	{
		target->join_queue = queue_create(THREAD_MAX_THREADS);
		queue_set_owner(target->join_queue, target);
	}

	int sleep = thread_sleep(target->join_queue);
	if (sleep == THREAD_DEADLOCK)
	{
		interrupt_set(previous_status);
		return sleep;
	}
	if (sleep == THREAD_NONE)
	{
		interrupt_set(previous_status);
		return sleep;
	}

	target = thread_get(tid);
	if (target)
	{
		if (exit_code)
		{
			*exit_code = target->exit_code;
		}
		if (!target->reaped && target->state == ZOMBIE && !target->w_exit)
		{
			target->reaped = true;
			thread_destroy(target);
		}
	}

	interrupt_set(previous_status);
	return tid;
}

Tid thread_sleep(fifo_queue_t *queue)
{
	assert(!interrupt_enabled());

	if (queue == NULL)
	{
		return THREAD_INVALID;
	}

	struct thread *holder = queue_get_owner(queue);
	while (holder != NULL)
	{
		if (holder == current)
		{
			return THREAD_DEADLOCK;
		}
		if (holder->wait_queue == NULL || holder->state != SLEEPING)
		{
			break;
		}
		holder = queue_get_owner(holder->wait_queue);
	}

	struct thread *next = scheduler->dequeue();
	if (next == NULL)
	{
		return THREAD_NONE;
	}

	current->state = SLEEPING;
	current->wait_queue = queue;
	queue_push(queue, current);
	Tid return_id = next->id;
	thread_switch(next);

	return return_id;
}

/* When the 'all' parameter is 1, wake up all threads waiting in the queue.
 * returns whether a thread was woken up on not.
 */
int thread_wakeup(fifo_queue_t *queue, int all)
{
	assert(!interrupt_enabled());
	int woken = 0;

	if (queue == NULL)
	{
		return 0;
	}

	if (all == 0)
	{
		struct thread *th = queue_pop(queue);
		if (th != NULL)
		{
			th->wait_queue = NULL;
			th->state = READY;
			scheduler->enqueue(th);
			woken = 1;
		}
	}
	else if (all == 1)
	{
		struct thread *th;
		while ((th = queue_pop(queue)) != NULL)
		{
			th->wait_queue = NULL;
			th->state = READY;
			scheduler->enqueue(th);
			woken++;
		}
	}

	return woken;
}

struct lock
{
	struct thread *holder;
	fifo_queue_t *wait_queue;
	int condition_variables;
};

struct lock *
lock_create()
{
	int prev = interrupt_set(0);

	struct lock *lock = malloc(sizeof(struct lock));
	if (lock == NULL)
	{
		interrupt_set(prev);
		return NULL;
	}
	lock->holder = NULL;
	lock->wait_queue = queue_create(THREAD_MAX_THREADS);
	lock->condition_variables = 0;
	queue_set_owner(lock->wait_queue, NULL);
	interrupt_set(prev);
	return lock;
}

void lock_destroy(struct lock *lock)
{
	assert(lock != NULL);
	int prev = interrupt_set(0);
	assert(lock->holder == NULL);
	assert(lock->condition_variables == 0);
	assert(queue_count(lock->wait_queue) == 0);
	queue_destroy(lock->wait_queue);
	free(lock);
	interrupt_set(prev);
}

int lock_acquire(struct lock *lock)
{
	assert(lock != NULL);
	int prev = interrupt_set(0);

	while (lock->holder != NULL)
	{
		int ret = thread_sleep(lock->wait_queue);

		if (ret == THREAD_DEADLOCK)
		{
			interrupt_set(prev);
			return ret;
		}
		if (ret == THREAD_NONE)
		{
			interrupt_set(prev);
			return ret;
		}
	}

	lock->holder = current;
	queue_set_owner(lock->wait_queue, lock->holder);
	interrupt_set(prev);
	return 0;
}

void lock_release(struct lock *lock)
{
	assert(lock != NULL);

	int prev = interrupt_set(0);
	assert(lock->holder != NULL);
	assert(lock->holder == current);
	lock->holder = NULL;
	queue_set_owner(lock->wait_queue, NULL);
	thread_wakeup(lock->wait_queue, 0);
	interrupt_set(prev);
}

struct cv
{
	struct lock *l;
	fifo_queue_t *wait_queue;
};

struct cv *
cv_create(struct lock *lock)
{
	struct cv *cv;
	int prev = interrupt_set(0);
	cv = malloc(sizeof(struct cv));
	cv->l = lock;
	cv->wait_queue = queue_create(THREAD_MAX_THREADS);
	queue_set_owner(cv->wait_queue, NULL);
	lock->condition_variables++;
	interrupt_set(prev);
	return cv;
}

void cv_destroy(struct cv *cv)
{
	int prev = interrupt_set(0);
	assert(queue_count(cv->wait_queue) == 0);
	cv->l->condition_variables--;
	queue_destroy(cv->wait_queue);
	free(cv);
	interrupt_set(prev);
}

int cv_wait(struct cv *cv)
{
	int prev = interrupt_set(0);
	assert(cv->l->holder == current);
	cv->l->holder = NULL;
	thread_wakeup(cv->l->wait_queue, 0);
	int sleep = thread_sleep(cv->wait_queue);
	if (sleep == THREAD_DEADLOCK)
	{

		interrupt_set(prev);
		return sleep;
	}
	if (sleep == THREAD_NONE)
	{
		interrupt_set(prev);
		return sleep;
	}

	int reacquire = lock_acquire(cv->l);
	if (reacquire < 0)
	{
		interrupt_set(prev);
		return reacquire;
	}

	interrupt_set(prev);
	return 0;
}

void cv_signal(struct cv *cv)
{
	int prev = interrupt_set(0);

	thread_wakeup(cv->wait_queue, 0);
	interrupt_set(prev);
}

void cv_broadcast(struct cv *cv)
{
	int prev = interrupt_set(0);

	thread_wakeup(cv->wait_queue, 1);
	interrupt_set(prev);
}
