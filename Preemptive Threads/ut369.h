/*
 * ut369.h
 *
 * Exported header file for UT369, a User-Level Threading Library
 * designed for CSC369 students.
 *
 *  DO NOT CHANGE
 */

#ifndef _UT369_H_
#define _UT369_H_

#include <stdbool.h>

#define THREAD_MAX_THREADS 1024 /* maximum number of threads */
#define THREAD_MIN_STACK  32768 /* minimum per-thread execution stack */

typedef int Tid; /* A thread identifier */

/*
 * Valid thread identifiers (Tid) range between 0 and THREAD_MAX_THREADS-1. The
 * first thread to run must have a thread id of 0. Note that this thread is the
 * main thread, i.e., it is created before the first call to thread_create.
 *
 * Negative Tid values are used for error codes or control codes.
 */
enum {
	THREAD_INVALID = -1,
	THREAD_ANY = -2,
	THREAD_NONE = -3,
	THREAD_NOMORE = -4,
	THREAD_NOMEMORY = -5,
	THREAD_DEADLOCK = -6,
	THREAD_TODO = -8,
	THREAD_KILLED = -9,
};

/* function type for a new thread's entry point */
typedef int (* thread_entry_f)(void *);

struct config {
    const char * sched_name;
    bool preemptive;
	bool verbose;
};

/*
 * Initializes the threading system (already implemented in ut369.c)
 * Must be called before using the threading system.
 */
void ut369_start(struct config * config);


/*
 * Return the thread identifier of the currently running thread.
 */
Tid thread_id(void);

/*
 * Create a new thread that is ready to start running the assigned function.
 *
 * Parameters:
 * - fn: A pointer to the function the new thread will execute.
 * - arg: An argument to be passed to the function `fn`.
 *
 * Behaviors:
 * - The created thread is placed in the ready queue and can be scheduled
 *   after this function returns.
 *
 * Return Values:
 * - On success: Returns the identifier of the newly created thread.
 * - On failure: Returns one of the following error codes:
 *   - THREAD_NOMORE: The system cannot create additional threads because
 *     the maximum thread limit has been reached.
 *   - THREAD_NOMEMORY: There is insufficient memory to allocate a stack
 *     or other resources required for the new thread.
 */
Tid thread_create(thread_entry_f fn, void *arg);

/*
 * Terminate the execution of the calling thread.
 *
 * Parameters:
 * - exit_code: The exit code of the calling thread.
 *
 * Behaviors:
 * - This function has no return value and permanently stops the execution of
 *   the calling thread.
 * - If other threads in the system are active and runnable, one of them should
 *   be scheduled to run. If no other threads exist (i.e., the calling thread is
 *   the last active thread), this function should call ut369_exit().
 * - (A2 only) If other threads are waiting for the exit code of the calling
 *   thread, i.e., via thread_wait, this thread's exit code must be made
 *   available to them.
 * - The identifier of the exiting thread cannot be reused until thread_wait
 *   is called on it.
 */
void thread_exit(int exit_code);

/*
 * Kill the thread specified by the identifier tid.
 *
 * Parameters:
 * - tid: Identifier of the thread to be killed. It must correspond to
 *        a valid and runnable thread, other than the calling thread.
 *
 * Behaviors:
 * - A killed thread will cease execution of its assigned function.
 * - The target thread is scheduled one final time to allow it to exit
 *   gracefully, upon which, its exit code will be set to THREAD_KILLED.
 * - If the specified thread is already killed or has exited, this
 *   function succeeds without any effect.
 * - The identifier of a killed thread cannot be reused until
 *   thread_wait is called on that thread (same as thread_exit).
 *
 * Return Values:
 * - Upon success, returns the identifier of the terminated thread.
 * - Upon failure, returns one of the following:
 *   - THREAD_INVALID: The provided tid does not correspond to a valid thread
 *     (e.g., it is a negative value, does not exist, or refers to the
 *     calling thread).
 */
Tid thread_kill(Tid tid);

/*
 * Suspend the execution of the calling thread and schedules the thread
 * identified by tid to run.
 *
 * Parameters:
 * - tid: Identifier of the thread to be scheduled. This can be the identifier
 *        of any available thread or one of the following constants:
 *        - THREAD_ANY: Schedule any thread from the ready queue.
 *
 * Behaviors:
 * - If the calling thread remains runnable, it is placed in the ready queue.
 * - Upon success, the function switches execution to the specified thread
 *   or a thread from the ready queue (if tid is THREAD_ANY). The function
 *   does not return to the calling thread until it is rescheduled later.
 * - If tid is invalid or no other threads are available to run, the calling
 *   thread continues execution, and an error code is returned.
 *
 * Return Values:
 * - On success, returns the identifier of the thread that was switched to.
 * - On failure, the calling thread continues running and one of the
 *   following is returned:
 *   - THREAD_INVALID: tid does not correspond to a valid and runnable thread.
 *   - THREAD_NONE: No threads, other than the caller, are available to run.
 *                  This only occurs if tid is THREAD_ANY.
 */
Tid thread_yield(Tid tid);

/**************************************************************************
 * (A2) API function and type declarations for preemptive threads only
 **************************************************************************/

/* forward declaration of type (defined in queue.c) */
struct _fifo_queue;
typedef struct _fifo_queue fifo_queue_t;

/*
 * Block the calling thread until the specified thread terminates, optionally
 * retrieving the thread's exit status.
 *
 * Parameters:
 * - tid: The identifier of the target thread to wait for.
 * - exit_code: A pointer to an integer where the exit status of the target
 *              thread will be stored. If NULL, the exit status is ignored.
 *
 * Behaviors:
 * - On success, suspends the execution of the calling thread until the
 *   target thread exits. If the target thread has already exited, the
 *   function returns immediately without blocking.
 * - If `exit_code` is not NULL, thread_wait() stores the exit status of the
 *   target thread (i.e., the value supplied to thread_exit) in the location
 *   pointed to by `exit_code`.
 * - If tid is invalid or no other threads are available to run, the calling
 *   thread continues execution, and an error code is returned.
 *
 * Return Values:
 * - On success: Returns the tid of the target thread, fully releasing the
 *   resources associated with the thread and allowing its identifier to be
 *   reused.
 * - On failure: The calling thread continues running, and one of the following
 *   error codes is returned:
 *   - THREAD_INVALID: one of the following situations occurred:
 *     - tid is not a valid thread identifier.
 *     - No thread exists with the specified tid.
 *     - tid refers to the calling thread.
 *     - The target thread has already exited AND another thread has already
 *       waited for it.
 *   - THREAD_DEADLOCK: a deadlock would occur if the current thread waits
 *     for the target thread.
 *   - THREAD_NONE: No other threads, aside from the caller, are available 
 *     to run.
 * 
 * Notes:
 * - If the target thread has not yet exited, multiple threads can wait
 *   for it.
 * - If the target thread has already exited, the calling thread can only
 *   wait for it if no other thread has waited for it yet.
 */
int thread_wait(Tid tid, int *exit_code);


/* forward declaration of type (defined in thread.c) */
struct lock;

/*
 * Create a blocking lock with First-Come, First-Served (FCFS) fairness.
 * The lock is initially available.
 * 
 * Hints: 
 * - Read ahead and think about what other member variables are needed
 *   to implement the full set of features required by A2.
 */
struct lock *lock_create(void);

/*
 * Destroy the lock.
 * 
 * Behaviors:
 * - This function shall crash the program with an assertion error if:
 *   - lock is currently held by a thread.
 *   - one or more condition variables are currently associated with it.
 *     (see cv_create for more information)
 *   - its wait queue is not empty.
 */
void lock_destroy(struct lock *lock);

/*
 * Acquire the lock. If the lock is unavailable, the calling thread is
 * placed in the lock's wait queue and suspended until it can acquire the lock.
 *
 * Return Values:
 * - 0 on success.
 * - THREAD_DEADLOCK: The calling thread cannot be suspended because doing so
 *   would result in a deadlock.
 * - THREAD_NONE: No other threads, aside from the caller, are available 
 *   to run.
 */
int lock_acquire(struct lock *lock);

/*
 * Release the lock and wakes up one other thread waiting to acquire the 
 * lock, if any. 
 * 
 * Behaviors:
 * - This function shall crash the program with an assertion error if:
 *   - lock is not currently held by any thread.
 *   - the caller is not the lock owner.
*/
void lock_release(struct lock *lock);


/* forward declaration of type (defined in thread.c) */
struct cv;

/*
 * Create a condition variable with First-Come, First-Served (FCFS) fairness.
 *
 * Parameters:
 * - lock: The lock to be associated with the cv. 
 */
struct cv *cv_create(struct lock * lock);

/*
 * Destroy the condition variable and remove its association with the
 * lock passed to it during cv_create.
 * 
 * Behaviours:
 * - This function shall crash the program with an assertion error if:
 *   - its wait queue is not empty.
 */
void cv_destroy(struct cv *cv);

/*
 * Suspend the calling thread on the condition variable cv.
 *
 * Behaviours:
 * - Release the associated lock before waiting, and re-acquire it before
 *   this function returns.
 * - This function shall crash the program with an assertion error if:
 *   - its associated lock is not held by the calling thread.
 * 
 * Return Values:
 * - 0 on success.
 * - THREAD_DEADLOCK: The calling thread cannot be suspended because doing so
 *   would result in a deadlock.
 * - THREAD_NONE: No other threads, aside from the caller, are available 
 *   to run.
 * 
 * Notes: 
 * - You are not required to handle the situation where the calling thread
 *   never wakes up due to incorrect use of the cv, e.g., lost wakeup.
 */
int cv_wait(struct cv *cv);

/*
 * Wake up one thread that is waiting on the condition variable cv.
 */
void cv_signal(struct cv *cv);

/*
 * Wake up all threads that are waiting on the condition variable cv.
 */
void cv_broadcast(struct cv *cv);


#endif /* _UT369_H_ */
