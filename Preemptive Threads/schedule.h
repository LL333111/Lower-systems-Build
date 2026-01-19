/*
 * schedule.h
 *
 * Internal Scheduler API declaration. DO NOT CHANGE
 *
 */

#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include <stdbool.h>

struct thread;

#define SCHEDULERS \
    S(rand) \
    S(fcfs) 

#define S(name) \
    int name ## _init(void); \
    int name ## _enqueue(struct thread *); \
    struct thread * name ## _dequeue(void); \
    struct thread * name ## _remove(Tid tid); \
    void name ## _destroy(void);
    SCHEDULERS
#undef S

struct scheduler {
    /* Name of the scheduler */
    const char * name;

    /* Scheduler's initialization code. Returns 0 on success, THREAD_NOMEMORY
     * upon error, e.g., out of memory. 
     */
    int (* init)(void);

    /* add a thread to the scheduler's ready queue. Returns 0 on success,
     * THREAD_NOMORE upon error, e.g., ready queue is full. 
     */
    int (* enqueue)(struct thread *);

    /* remove the best thread from the ready queue, in accordance to the
     * scheduler's goal(s). Returns NULL if the ready queue is empty. 
     */
    struct thread * (* dequeue)(void);
    
    /* remove a thread with matching tid. Returns NULL if thread with the 
     * specified tid cannot be found.
     */
    struct thread * (* remove)(Tid tid);
    void (* destroy)(void);
};

extern struct scheduler * scheduler;

bool scheduler_init(const char *);
void scheduler_end(void);


#endif /* _SCHEDULE_H_ */