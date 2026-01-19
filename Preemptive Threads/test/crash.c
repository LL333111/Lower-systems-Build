#include "timeout.h"
#include "test.h"

static inline void
thread_expect(bool cond)
{
	if (!cond) {
		exit(EXIT_FAILURE);
	}
}

/* -------------------global variables ------------------ */

static struct lock *testlock;
static struct cv *testcv;

/* --------------------- test cases --------------------- */

int
test_lock_destroy_held_by_thread(void)
{
    testlock = lock_create();
    int ret = lock_acquire(testlock);
    thread_expect(ret == 0);

    // Should just crash here
    lock_destroy(testlock);  

    // Test failed if it didn't crash
    return 0;
}

int
test_lock_destroy_cv_associated(void)
{
    testlock = lock_create();
    testcv = cv_create(testlock);
    
    // Should just crash here
    lock_destroy(testlock);  

    // No op for cv, to make the compiler happy
    (void)testcv;

    // Test failed if it didn't crash
    return 0;
}

int
test_lock_release_not_held(void)
{
    testlock = lock_create();

    // Should just crash here
    lock_release(testlock);

    // Test failed if it didn't crash
    return 0;
}

static bool locked;

static
void
new_thread_lock_acquire(struct lock *lock)
{
    int ret = lock_acquire(lock);
    thread_expect(ret == 0);

    // We acquired the lock -- can test now
    locked = true;

    // Yield back to the main thread
    while (1) {
        ret = thread_yield(THREAD_ANY);
        thread_expect(ret >= 0);
    }
}

int
test_lock_release_not_owner(void)
{
    locked = false;
    testlock = lock_create();

    // Create a new thread, s.t. its only purpose is to acquire the lock
    int ret = thread_create((thread_entry_f) new_thread_lock_acquire, testlock);
    thread_expect(ret >= 0);

    while (!locked) {
        ret = thread_yield(THREAD_ANY);
        thread_expect(ret >= 0);
    }

    // Should just crash here
    lock_release(testlock);  

    // Test failed if it didn't crash
    return 0;
}

int
test_cv_wait_lock_not_owner(void) 
{
    locked = false;
    testlock = lock_create();
    testcv = cv_create(testlock);

    // Create a new thread that holds the lock
    int ret = thread_create((thread_entry_f) new_thread_lock_acquire, testlock);
    thread_expect(ret >= 0);

    while (!locked) {
        ret = thread_yield(THREAD_ANY);
        thread_expect(ret >= 0);
    }

    // Should just crash here
    cv_wait(testcv);  

    // Test failed if it didn't crash
    return 0;
}

static void
new_thread_cv_wait(void)
{
    int ret = lock_acquire(testlock);
    thread_expect(ret == 0);

    while (1) {
        ret = cv_wait(testcv);
        thread_expect(ret == 0);
    }
}

int
test_cv_wait_queue_not_empty(void)
{
    testlock = lock_create();
    testcv = cv_create(testlock);

    // Create a new thread that holds the lock then wait on the cv
    int ret = thread_create((thread_entry_f) new_thread_cv_wait, NULL);
    thread_expect(ret >= 0);

    // wait until child thread blocks
    while (ret != THREAD_NONE) {
        ret = thread_yield(THREAD_ANY);
    }

    // Should just crash here
    cv_destroy(testcv);  

    // Test failed if it didn't crash
    return 0;
}

int
test_sleep_interrupt_enabled(void) 
{
    fifo_queue_t *queue = queue_create(1);

    // Should just crash here
    thread_sleep(queue);  

   // Test failed if it didn't crash
    return 0;
}

int
test_wakeup_interrupt_enabled(void) 
{
    fifo_queue_t *queue = queue_create(1);

    // Should just crash here
    thread_wakeup(queue, 0);  

    // Test failed if it didn't crash
    return 0;
}

testcase_t test_case[] = {
    { "Lock Destroy - Held by thread", test_lock_destroy_held_by_thread },
    { "Lock Destroy - CV associated", test_lock_destroy_cv_associated },
    { "Lock Release - Not held", test_lock_release_not_held },
    { "Lock Release - Not owner", test_lock_release_not_owner },
    { "CV Wait - Lock Not held", test_cv_wait_lock_not_owner },
    { "CV Destroy - Queue not empty", test_cv_wait_queue_not_empty },
    { "Thread Sleep - Interrupt enabled", test_sleep_interrupt_enabled },
    { "Thread Wakeup - Interrupt enabled", test_wakeup_interrupt_enabled },
};

int nr_cases = sizeof(test_case) / sizeof(struct _tc);
const int timeout_secs = 5;

int
run_test_case(int test_id)
{
    struct config config = { 
        .sched_name = "rand", .preemptive = true, .verbose = false
    };

    ut369_start(&config);
    return test_case[test_id].func();
}

void
wait_process(pid_t child_pid)
{
    int status = selfpipe_waitpid(child_pid, timeout_secs);
    if (WIFEXITED(status)) {
        printf("EXIT BY CRASH? No\n");
    } else if (WIFSIGNALED(status)) {
        int signum = WTERMSIG(status);
        if (signum == SIGABRT) {
            printf("EXIT BY CRASH? Yes\n");
        }
        else {
            printf("TIMEOUT (%d)\n", signum);
        }
    }
    else {
        printf("UNKNOWN\n");
    }
}

int
main(int argc, const char * argv[])
{  
    return main_process("crash", argc, argv);
}