#include "timeout.h"
#include "test.h"

#define NUM_THREADS 15

int ready;
static struct lock **lock_array;
static struct lock *lock1, *lock2;
static struct cv *cv;

static int
simple_deadlock_1(Tid *tid)
{
    int ret;
    ret = lock_acquire(lock1);
    assert(ret == 0);
    ret = thread_yield(*tid);
    assert(ret == *tid);
    ret = lock_acquire(lock2);
    // If we acquired the lock, make sure we release it.
    if (ret == 0)
        lock_release(lock2);
    lock_release(lock1);
    return ret;
}

static int
simple_deadlock_2(Tid *tid)
{
    int ret;
    ret = lock_acquire(lock2);
    assert(ret == 0);
    ret = thread_yield(*tid);
    assert(ret == *tid);
    ret = lock_acquire(lock1);
    // If we acquired the lock, make sure we release it.
    if (ret == 0)
        lock_release(lock1);
    lock_release(lock2);
    return ret;
}

static int
thread_waiter(Tid *tid)
{
    int ret = 0;
    ret = thread_wait(*tid, NULL);
    return ret;
}

static int
thread_wait_yield(Tid *tid)
{
    int ret = 0;
    ret = thread_wait(*tid, NULL);
    /* This may result in THREAD_NONE */
    thread_yield(THREAD_ANY);
    return ret;
}

static int
double_lock_waiter(void)
{
    int ret;
    ret = lock_acquire(lock1);
    assert(ret == 0);
    ret = lock_acquire(lock2);
    // If we acquired the lock, make sure we release it.
    if (ret == 0)
        lock_release(lock2);
    // Wake up cv waiters
    cv_broadcast(cv);
    lock_release(lock1);
    return ret;
}

static int
cv_waiter(struct cv *cv)
{
    int ret = lock_acquire(lock1);
    assert(ret == 0);
    ret = cv_wait(cv);
    if (ret == 0)
        lock_release(lock1);
    return ret;
}

static int
lock_acquirer(struct lock *lock)
{
    int ret = lock_acquire(lock);
    if (ret == 0)
        lock_release(lock);
    return ret;
}

static int
cv_deadlocker(Tid *tid)
{
    int ret = lock_acquire(lock1);
    assert(ret == 0);
    cv_signal(cv);
    int exit_code;
    ret = thread_wait(*tid, &exit_code);
    assert(ret == *tid);
    lock_release(lock1);
    return exit_code;
}

static int
lock_yield_wait(Tid *tid)
{
    int ret = lock_acquire(lock1);
    assert(ret == 0);
    ret = thread_yield(THREAD_ANY);
    assert(ret >= 0 || ret == THREAD_NONE);
    ret = thread_wait(*tid, NULL);
    lock_release(lock1);
    return ret;
}

static int
test_circular_lock_holding(void)
{
    long tid1, tid2;
    int exit_code1, exit_code2;
    Tid ret;

    // Testing deadlock caused by lock holding.
    lock1 = lock_create();
    lock2 = lock_create();
    tid1 = thread_create((thread_entry_f)simple_deadlock_1, (void*)&tid2);
    tid2 = thread_create((thread_entry_f)simple_deadlock_2, (void*)&tid1);

    ret = thread_wait(tid1, &exit_code1);
    assert(ret == tid1);
    ret = thread_wait(tid2, &exit_code2);
    assert(ret == tid2);

    // Only one of the threads should deadlock and fail, not both.
    assert((exit_code1 == THREAD_DEADLOCK) != (exit_code2 == THREAD_DEADLOCK));

    // Clean up
    lock_destroy(lock2);
    lock_destroy(lock1);

    return 0;
}

static int
test_circular_wait(void)
{
    long tid1, tid2;
    int exit_code;
    Tid ret;

    // Testing deadlock caused by simple circular wait.
    tid1 = thread_create((thread_entry_f)thread_waiter, (void*)&tid2);
    tid2 = thread_create((thread_entry_f)thread_wait_yield, (void*)&tid1);

    // Yield to first thread so it blocks on tid2
    ret = thread_yield(tid1);
    assert(ret == tid1);

    // Wait for tid2, which should always deadlock trying to wait for tid1
    ret = thread_wait(tid2, &exit_code);
    assert(ret == tid2 && exit_code == THREAD_DEADLOCK);

    // Clean up
    ret = thread_wait(tid1, &exit_code);
    assert(ret == tid1 && exit_code == tid2);

    return 0;
}

static int
test_extensive_circular_wait(void)
{
    Tid ret;
    Tid tids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS - 1; i++) {
        // Make all threads wait for the next thread in the list.
        tids[i] = thread_create((thread_entry_f)thread_waiter, &tids[i + 1]);
    }

    int tmp = thread_id();
    tids[NUM_THREADS - 1] = thread_create((thread_entry_f)thread_waiter, &tmp);

    // Make all the threads start waiting.
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_yield(tids[i]);
    }

    int num_deadlock = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        ret = thread_wait(tids[i], NULL);
        if (ret == THREAD_DEADLOCK)
            num_deadlock++;
    }

    // All the threads should have caused a deadlock, because the current
    // thread waiting on them should cause a circular wait (because starting
    // from thread x, x waits on x+1, x+1 waits on x+2... x+n waits on 0).
    assert(num_deadlock == NUM_THREADS);

    // Clean up
    for (int i = 0; i < NUM_THREADS; i++) {
        ret = thread_kill(tids[i]);
        assert(ret == tids[i]);
        ret = thread_wait(tids[i], NULL);
        assert(ret == tids[i]);
    }

    return 0;
}

static int
test_wait_on_lock_waiter(void)
{
    long tid1, tid2;
    int exit_code1, exit_code2;
    Tid ret;

    // Testing deadlock caused by waiting on a thread that is waiting on a lock
    // that you hold.
    lock1 = lock_create();
    tid1 = thread_create((thread_entry_f)lock_yield_wait, (void*)&tid2);
    tid2 = thread_create((thread_entry_f)thread_waiter, (void*)&tid1);

    ret = thread_wait(tid1, &exit_code1);
    assert(ret == tid1);
    ret = thread_wait(tid2, &exit_code2);
    assert(ret == tid2);

    // Only one of the threads should deadlock and fail, not both.
    assert((exit_code1 == THREAD_DEADLOCK) != (exit_code2 == THREAD_DEADLOCK));

    // Clean up
    lock_destroy(lock1);

    return 0;
}

static int
test_cv_wait_on_waiter(void)
{
    long tid1, tid2;
    int exit_code2;
    Tid ret;

    // cv_wait causing deadlock (other thread calls signal, waits for the
    // thread waiting on the cv before freeing the lock).
    lock1 = lock_create();
    cv = cv_create(lock1);
    tid1 = thread_create((thread_entry_f)cv_waiter, (void*)cv);
    tid2 = thread_create((thread_entry_f)cv_deadlocker, (void*)&tid1);

    // Make tid1 wait for the cv.
    ret = thread_yield(tid1);
    assert(ret == tid1);

    // Sleep waiting for tid2. This should cause the deadlock.
    ret = thread_wait(tid2, &exit_code2);
    assert(ret == tid2);

    // Only the second thread should fail.
    assert(exit_code2 == THREAD_DEADLOCK);

    // Clean up
    cv_destroy(cv);
    lock_destroy(lock1);

    return 0;
}

static int
test_cv_wait_no_runnable(void)
{
    long tid;

    // 2 cv_wait resulting in THREAD_NONE
    lock1 = lock_create();
    cv = cv_create(lock1);
    tid = thread_create((thread_entry_f)cv_waiter, (void*)cv);

    int ret = thread_yield(tid);
    assert(ret == tid);
    ret = lock_acquire(lock1);
    assert(ret == 0);

    // This should fail, because we would have no threads to run.
    ret = cv_wait(cv);
    assert((ret == THREAD_NONE));

    // wake up cv_waiter
    cv_broadcast(cv);
    ret = thread_wait(tid, NULL);
    assert(ret == tid);
    cv_destroy(cv);
    lock_destroy(lock1);

    return 0;
}

static int
test_lock_no_runnable(void)
{
    long tid1, tid2;
    int exit_code1, exit_code2;
    Tid ret;

    lock1 = lock_create();
    lock2 = lock_create();
    cv = cv_create(lock1);

    // Make tid1 wait for the cv.
    tid1 = thread_create((thread_entry_f)cv_waiter, (void*)cv);
    assert(tid1 >= 0);
    ret = thread_yield(tid1);
    assert(ret == tid1);

    // Wait for tid2 to run.
    tid2 = thread_create((thread_entry_f)lock_acquirer, (void*)lock2);
    assert(tid2 >= 0);
    ret = lock_acquire(lock2);
    assert(ret == 0);
    ret = thread_wait(tid2, &exit_code2);
    assert(ret == tid2);
    lock_release(lock2);

    // Signal tid1 and make it come back.
    cv_signal(cv);
    ret = thread_wait(tid1, &exit_code1);
    assert(ret == tid1);

    // The second thread should have caught the deadlock.
    assert((exit_code1 == 0) && (exit_code2 == THREAD_DEADLOCK));

    // Clean up
    cv_destroy(cv);
    lock_destroy(lock2);
    lock_destroy(lock1);

    return 0;
}

static int
test_wait_no_runnable(void)
{
    long tid1, tid2;
    int exit_code;
    Tid ret;

    // 1 cv_wait and 1 thread_wait resulting in THREAD_NONE
    lock1 = lock_create();
    cv = cv_create(lock1);

    // create first thread and run it (so that it blocks)
    tid1 = thread_create((thread_entry_f)cv_waiter, (void*)cv);
    assert(tid1 >= 0);
    ret = thread_yield(tid1);
    assert(ret == tid1);

    // create second thread and let it run
    tid2 = thread_create((thread_entry_f)thread_waiter, (void*)&tid1);
    assert(tid2 >= 0);
    ret = thread_wait(tid2, &exit_code);
    assert(ret == tid2);

    // The second thread we created should catch the deadlock.
    assert(exit_code == THREAD_NONE);

    // Clean up
    cv_broadcast(cv);
    ret = thread_wait(tid1, NULL);
    assert(ret == tid1);
    cv_destroy(cv);
    lock_destroy(lock1);

    return 0;
}

static int
test_triangular_wait(void)
{
    long tid1, tid2;
    int exit_code;
    Tid ret;

    lock1 = lock_create();
    lock2 = lock_create();
    cv = cv_create(lock1);


    // create first thread and let it block on cv
    tid1 = thread_create((thread_entry_f)cv_waiter, (void*)cv);
    assert(tid1 >= 0);
    ret = thread_yield(tid1);
    assert(ret == tid1);

    // Acquire lock2, so that double lock waiter will block
    ret = lock_acquire(lock2);
    assert(ret == 0);

    // Create second thread and let it block on lock2
    tid2 = thread_create((thread_entry_f)double_lock_waiter, NULL);
    assert(tid2 >= 0);
    ret = thread_yield(tid2);
    assert(ret == tid2);

    // We should now have a circular wait of three threads
    ret = thread_wait(tid1, NULL);
    assert(ret == THREAD_DEADLOCK);

    // Clean up
    lock_release(lock2);
    ret = thread_wait(tid2, &exit_code);
    assert(ret == tid2 && exit_code == 0);
    ret = thread_wait(tid1, &exit_code);
    assert(ret == tid1 && exit_code == 0);
    cv_destroy(cv);
    lock_destroy(lock2);
    lock_destroy(lock1);

    return 0;
}

static int
lock_yield_lock(int index)
{
    int ret;
    struct lock * lockA, * lockB;
    
    lockA = lock_array[index];
    ret = lock_acquire(lockA);
    assert(ret == 0);
    
    // Atomically increment ready by 1
    __sync_fetch_and_add(&ready, 1);

    // Wait for others to acquire their first lock
    while (ready < NUM_THREADS + 1) {
        ret = thread_yield(THREAD_ANY);
        assert(ret >= 0);
    }

    lockB = lock_array[index+1];
    ret = lock_acquire(lockB);
    assert(ret == 0);
    lock_release(lockB);
    lock_release(lockA);

    return 0;
}

static int
test_extensive_lock_wait(void)
{
    long i;
    int ret;
    Tid tids[NUM_THREADS];
    ready = 0;
    const int lock_array_size = sizeof(struct lock *) * (NUM_THREADS + 1);
    lock_array = malloc(lock_array_size);
    memset(lock_array, 0, sizeof(lock_array_size));

    for (i = 0; i < NUM_THREADS; i++) {
        lock_array[i] = lock_create();
        assert(lock_array[i]);
        tids[i] = thread_create((thread_entry_f)lock_yield_lock, (void *)i);
        assert(tids[i] >= 0);
        ret = thread_yield(tids[i]);
        assert(ret == tids[i]);
    }

    // Create and acquire the lock for main thread
    lock_array[i] = lock_create();
    assert(lock_array[i]);
    ret = lock_acquire(lock_array[i]);
    assert(ret == 0);
    __sync_fetch_and_add(&ready, 1);

    // Let all threads block on their second lock_acquire
    do {
        ret = thread_yield(THREAD_ANY);
    } while (ret != THREAD_NONE);

    // This should cause circular lock holding
    ret = lock_acquire(lock_array[0]);
    assert(ret == THREAD_DEADLOCK);

    // Clean up
    lock_release(lock_array[i]);

    for (i = 0; i < NUM_THREADS; i++) {
        ret = thread_wait(tids[i], NULL);
        assert(ret == tids[i]);
    }

    for (i = NUM_THREADS; i >= 0; i--) {
        lock_destroy(lock_array[i]);
    }

    free(lock_array);
    return 0;
}

testcase_t test_case[] = {
    { "Circular Lock Holding", test_circular_lock_holding },
    { "Circular Wait", test_circular_wait },
    { "Extensive Circular Wait", test_extensive_circular_wait },
    { "Wait on Waiter of Your Lock", test_wait_on_lock_waiter },
    { "CV Wait on Waiter", test_cv_wait_on_waiter },
    { "CV Wait - No Runnable Threads", test_cv_wait_no_runnable },
    { "Lock Acquire - No Runnable Threads", test_lock_no_runnable },
    { "Thread Wait - No Runnable Threads", test_wait_no_runnable },
    { "Circular Wait - 3 Threads", test_triangular_wait },
    { "Extensive Circulr Lock Holding", test_extensive_lock_wait },
};

int nr_cases = sizeof(test_case) / sizeof(struct _tc);
const int timeout_secs = 5;

int
run_test_case(int test_id)
{
    struct config config = { 
        .sched_name = "rand", .preemptive = false, .verbose = false
    };

    ut369_start(&config);
    int ret = test_case[test_id].func();
    thread_exit(ret);
    assert(0);
    return ret;
}

void
wait_process(pid_t child_pid)
{
    int status = selfpipe_waitpid(child_pid, timeout_secs);
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == 0) {
            printf("PASSED\n");
        }
        else {
            printf("FAILED (-%d)\n", code);
        }
    } else if (WIFSIGNALED(status)) {
        int signum = WTERMSIG(status);
        if (signum == SIGKILL) {
            printf("TIMEOUT\n");
        }
        else {
            printf("FAILED (%d)\n", signum);
        }
    }
    else {
        printf("UNKNOWN\n");
    }
}

int
main(int argc, const char * argv[])
{  
    return main_process("deadlock", argc, argv);
}