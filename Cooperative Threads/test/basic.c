#include "../ut369.h"
#include <assert.h>
#include <stdio.h>

#define SECRET 42

int main() 
{
    struct config config = { 
        .sched_name = "rand", 
        .preemptive = false 
    };

    ut369_start(&config);
    printf("initial thread's tid: %d\n", thread_id());
    printf("exiting initial thread with exit code %d\n", SECRET);
    thread_exit(SECRET);
    assert(false);
    return SECRET;
}