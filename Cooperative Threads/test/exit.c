#include "../ut369.h"
#include <assert.h>
#include <stdio.h>
#include <malloc.h>

/*
 * This file tests the freeing of stack after a thread exits (and becomes zombie).
 *
 * 1. Tests stack freeing in the second half of thread_yield
 * 2. Tests stack freeing at the beginning of thread_stub
 */

struct mallinfo minfo;
size_t allocated_space;

void caretaker(void)
{
    minfo = mallinfo();
	if (allocated_space <= (size_t)minfo.uordblks) {
		printf("2. it appears that a thread's stack is not freed after exit\n");
	}
    else {
        printf("2. stack appears to be freed after exit.\n");
    }
}

void create_then_exit(void)
{
    Tid tid = thread_create((thread_entry_f)caretaker, 0);
    assert(tid >= 0);

    minfo = mallinfo();
    allocated_space = minfo.uordblks;

    /* this thread would exit and caretake would free its stack */
}

int main() 
{
    struct config config = { 
        .sched_name = "rand", 
        .preemptive = false 
    };

    ut369_start(&config);

    /* create a thread that immediately exits */
    Tid tid = thread_create((thread_entry_f)thread_exit, 0);
    assert(tid >= 0);

	minfo = mallinfo();
	allocated_space = minfo.uordblks;

    int ret = thread_yield(tid);
    assert(ret == tid);

	minfo = mallinfo();
	if (allocated_space <= (size_t)minfo.uordblks) {
		printf("1. it appears that a thread's stack is not freed after exit\n");
	}
    else {
        printf("1. stack appears to be freed after exit.\n");
    }
	
    /* reap exited thread */
    ret = thread_wait(tid, NULL);
    printf("wait(%d) = %d\n", tid, ret);

    tid = thread_create((thread_entry_f)create_then_exit, 0);
    assert(tid >= 0);
    

    thread_exit(0);
    assert(false);
    return 0;
}