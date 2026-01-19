/*
 * schedule.c
 *
 * Implementation of a generic scheduler. DO NOT CHANGE.
 *
 */

#include "ut369.h"
#include "thread.h"
#include "interrupt.h"
#include "schedule.h"
#include <string.h>
#include <assert.h>

// current scheduler
struct scheduler * scheduler;

// set of available schedulers
struct scheduler schedulers[] = {
#define S(name) \
    { #name, name ## _init, name ## _enqueue, name ## _dequeue, name ## _remove, name ## _destroy },
    SCHEDULERS
#undef S
};

const int num_schedulers = sizeof(schedulers)/sizeof(struct scheduler);

/* initialize the scheduling subsystem */
bool 
scheduler_init(const char * name)
{
    scheduler = NULL;
    for (int i = 0; i < num_schedulers; i++) {
        if (strcmp(name, schedulers[i].name) == 0) {
            scheduler = &schedulers[i];
            return scheduler->init();
        }
    }
    
    return false;
}

/* clean up the scheduling subsystem */
void 
scheduler_end(void)
{
    if (scheduler != NULL) {
        scheduler->destroy();
    }
    scheduler = NULL;
}

