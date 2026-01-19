#include "../ut369.h"
#include <assert.h>
#include <stdio.h>

#define NUM_THREADS 10

Tid thread_ids[NUM_THREADS] = {0};
int num_checked = 0;

void check_my_id(void * arg)
{
    int i = (long)arg;

    if (thread_ids[i] != thread_id()) {
        printf("error: thread %d should run next\n", i);
        assert(false);
    }

    num_checked++;
    thread_yield(THREAD_ANY);
    printf("error: should not return to thread %d after thread_yield\n", i);
    assert(false);
}

int main() 
{
    struct config config = { 
        .sched_name = "fcfs", 
        .preemptive = false 
    };

    ut369_start(&config);
    
    for (int i = 1; i < NUM_THREADS; i++) {
        Tid ret = thread_create((thread_entry_f)check_my_id, (void *)(long)i);
        assert(ret >= 0);
        thread_ids[i] = ret;
    }

    num_checked = 0;
    Tid ret = thread_yield(THREAD_ANY);
    assert(ret >= 0);
    assert(num_checked == NUM_THREADS - 1);

    // kill all threads
    for (int i = 1; i < NUM_THREADS; i++) {
        Tid ret = thread_kill(thread_ids[i]);
        assert(ret == thread_ids[i]);
    }

    // reap all zombies
    for (int i = 1; i < NUM_THREADS; i++) {
        int ret = thread_wait(thread_ids[i], NULL);
        assert(ret == 0);
    }

    printf("FCFS scheduler is working.\n");
    thread_exit(0);
    assert(false);
    return 0;
}