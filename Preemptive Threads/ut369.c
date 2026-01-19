#include "ut369.h"
#include "interrupt.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>

/* Exit status of the process */
static int exit_status = 0;

/* Context of main thread for future clean-up */
static ucontext_t main_context;     

/* This must be here because we return from ut369_start, which means
 * there will be a stack mismatch when we restore context. To deal with
 * this, we make this variable global instead.
 */
static volatile int called = 0;     

/* The one end function that calls all other end functions before 
 * exiting the process with exit_status.
 */
static void 
ut369_end(void)
{
    assert(!interrupt_enabled());
    interrupt_end();
    thread_end();
    scheduler_end();
    exit(exit_status);
}

/* Start the ut369 user thread system using the given configuration
 * settings.
 */
void 
ut369_start(struct config * config)
{
    srand(0);
    scheduler_init(config->sched_name);
    thread_init();
    if (config->preemptive)
        interrupt_init(config->verbose ? 1 : 0);
    
    // make sure interrupt is off before we getcontext
    assert(!interrupt_enabled());

    // save main thread's context so we can exit the system gracefully
    getcontext(&main_context);
    if (called == 1) {
        // arrived here from ut369_exit
        ut369_end();
        assert(false);
    }
    called = 1;

    // interrupt is enabled from this point forward
    interrupt_on();
}

/* Cleans up resources used by this user thread library and exits the
 * process with exit_code. Should be called by thread_exit and not 
 * directly by the library user.
 */
void ut369_exit(int exit_code)
{
    assert(!interrupt_enabled());
    exit_status = exit_code;

    // context switch to main thread and run ut369_end function
    setcontext(&main_context);
    assert(false);
}
