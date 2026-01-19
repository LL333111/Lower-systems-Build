#include "../ut369.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#define NTHREADS 128

static inline int
thread_ret_ok(Tid ret)
{
	return (ret >= 0 ? 1 : 0);
}

int check_my_tid(void * arg)
{
    Tid my_tid = *((Tid *)arg);
    printf("my_tid: %d, actual = %d\n", my_tid, thread_id());
    return 0;
}

void check_my_rbp(const char * name)
{
    int ret = thread_id();
    char str[28];

    printf("hello, %s\n", name);

	/* we cast ret to a float because that helps to check
	 * whether the stack alignment of the frame pointer is correct */
	sprintf(str, "%3.0f", (float)ret);

    while (true) {
        ret = thread_yield(THREAD_ANY);

        /* thread 0 should be alive throughout this test */
       assert(thread_ret_ok(ret >= 0));
    }
}

static Tid tid_in_use[THREAD_MAX_THREADS] = {0};

static Tid
my_thread_create(thread_entry_f fn, void * arg)
{
    Tid ret = thread_create(fn, arg);
    
    if (ret < 0) {
        return ret;
    }

    assert(ret < THREAD_MAX_THREADS);
    assert(tid_in_use[ret] == false);
    tid_in_use[ret] = true;
    return ret;
}

static void 
my_thread_wait(Tid tid)
{
    int ret;
    
    assert(tid_in_use[tid] == true);
    ret = thread_wait(tid, NULL);
    assert(ret == 0);
    
    tid_in_use[tid] = false;
}

long *stack_array[THREAD_MAX_THREADS];

static int
fact(int n)
{
	/* store address of some variable on stack */
	stack_array[thread_id()] = (long *)&n;
	if (n == 1) {
		return 1;
	}
	return n * fact(n - 1);
}

/* thread 1 will take over half-way thru the test */
static int thread_1_main(void * arg);

int main() 
{
    size_t allocated_space;
    struct config config = { 
        .sched_name = "rand", 
        .preemptive = false 
    };

    ut369_start(&config);
    assert(thread_id() == 0);
    tid_in_use[0] = true;
 
    struct mallinfo minfo;
	minfo = mallinfo();
	allocated_space = minfo.uordblks;
	/* create a thread */
    Tid tid;
	tid = my_thread_create(check_my_tid, &tid);
	minfo = mallinfo();
	if ((size_t)minfo.uordblks <= allocated_space) {
		printf("it appears that the thread stack is not being"
		       "allocated dynamically\n");
	}
    else {
        printf("stack appears to be dynamically allocated.\n");
    }

    int ret = thread_yield(tid);
    assert(ret == tid);
    my_thread_wait(tid);

	/* store address of some variable on stack */
	stack_array[thread_id()] = (long *)&ret;

	int ii, jj;
	/* we will be using THREAD_MAX_THREADS threads later */
	Tid child[THREAD_MAX_THREADS];
	char msg[NTHREADS][64];

	/* create NTHREADS threads */
	for (ii = 0; ii < NTHREADS; ii++) {
		ret = snprintf(msg[ii], sizeof(msg[0])-1, "thread %2d", ii);
		assert(ret > 0);
		child[ii] = my_thread_create((thread_entry_f)check_my_rbp, msg[ii]);
		assert(thread_ret_ok(child[ii]));
	}

	for (ii = 0; ii < NTHREADS; ii++) {
		ret = thread_yield(child[ii]);
		assert(ret == child[ii]);
	}

	/* reap then destroy NTHREADS + 1 threads we just created */
	for (ii = 0; ii < NTHREADS; ii++) {
		ret = thread_kill(child[ii]);
		assert(ret == child[ii]);
		my_thread_wait(child[ii]);
	}

    printf("rbp alignment test completed.\n");

    /*
	 * create maxthreads-1 threads
	 */
	printf("creating %d threads\n", THREAD_MAX_THREADS - 1);
	for (ii = 0; ii < THREAD_MAX_THREADS - 1; ii++) {
		child[ii] = my_thread_create((thread_entry_f)fact, (void *)10);
		assert(thread_ret_ok(child[ii]));
	}

	/*
	 * Now we're out of threads. Next create should fail.
	 */
	ret = my_thread_create((thread_entry_f)fact, (void *)10);
	assert(ret == THREAD_NOMORE);

	/*
	 * Now let them all run.
	 */
	printf("running %d threads\n", THREAD_MAX_THREADS - 1);
	for (ii = 0; ii < THREAD_MAX_THREADS; ii++) {
		ret = thread_yield(ii);
		if (ii == 0) {
			/* 
			 * Guaranteed that first yield will find someone. 
			 * Later ones may or may not depending on who
			 * stub schedules on exit.
			 */
			assert(thread_ret_ok(ret));
		}
	}

	/* check that the thread stacks are sufficiently far apart */
	for (ii = 0; ii < THREAD_MAX_THREADS; ii++) {
		for (jj = ii + 1; jj < THREAD_MAX_THREADS; jj++) {
			long stack_sep = (long)(stack_array[ii]) -
				(long)(stack_array[jj]);
			if ((labs(stack_sep) < THREAD_MIN_STACK)) {
				printf("stacks of threads %d and %d "
				       "are too close\n", ii, jj);
				assert(0);
			}
		}
	}

    /*
	 * Reap zombies
	 */
	printf("reaping %d threads\n", THREAD_MAX_THREADS - 1);
	for (ii = 0; ii < THREAD_MAX_THREADS - 1; ii++) {
		my_thread_wait(child[ii]);
	}

    printf("creating thread_1_main\n");;
	ret = my_thread_create((thread_entry_f)thread_1_main, (void *)(long)thread_id());
	assert(thread_ret_ok(ret));

    thread_exit(0);
    assert(false);
    return 0;
}

static int
shoulder_stand(void * arg)
{
    Tid parent_tid = (long)arg;
    Tid child_tid;
    
    printf("thread %d created thread %d\n", parent_tid, thread_id());
    child_tid = my_thread_create(shoulder_stand, (void *)(long)thread_id());
    if (child_tid != THREAD_NOMORE)
        my_thread_wait(child_tid);
    else
        printf("thread limit reached at thread %d\n", thread_id());
    return thread_id();
}

static int 
thread_1_main(void * arg)
{
    int init_tid = (long)arg;
    my_thread_wait(init_tid);
    printf("init thread reaped\n");

    int ret = my_thread_create(shoulder_stand, (void *)(long)thread_id());
	assert(thread_ret_ok(ret));
	my_thread_wait(ret);

    const int secret = 42;
    printf("exiting program with code %d\n", secret);
    return secret;
}