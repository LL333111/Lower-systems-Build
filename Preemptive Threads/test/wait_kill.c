#include "test.h"

static int
test_wait_kill_thread(Tid parent)
{
	Tid ret;
	int exitcode;

	/* Only have 2 threads in this test, child and parent.
	 * Child yields until there are no other runnable threads, to
	 * ensure that parent has executed its thread_wait() on the child. */
	while (thread_yield(THREAD_ANY) != THREAD_NONE);

	/* Parent thread is now blocked, waiting on this child. */
	ret = thread_kill(parent);	/* ouch */
	if (ret != parent) {
		unintr_printf("%s: bad thread_kill, expected %d got %d\n",
			      __FUNCTION__, parent, ret);
	}
	/* Wait for the killed parent! */
	ret = thread_wait(parent, &exitcode);
	if (ret != parent) {
		unintr_printf("%s: bad thread_wait, expected %d got %d\n",
			      __FUNCTION__, parent, ret);
	}

	/* killed thread should have an exit code of THREAD_KILLED */
	assert(exitcode == THREAD_KILLED);
	unintr_printf("wait_kill test done\n");
	return exitcode;
}

int
main()
{
	Tid child;
	Tid ret;

	printf("starting wait_kill test\n");

   struct config config = { 
        .sched_name = "rand", .preemptive = true, .verbose = false
    };
	ut369_start(&config);

	/* create a thread */
	child = thread_create((thread_entry_f)test_wait_kill_thread,
			      (void *)(long)thread_id());
	assert(thread_ret_ok(child));
	ret = thread_wait(child, NULL);

	/* child kills parent! we shouldn't get here */
	assert(ret == child);
	unintr_printf("wait_kill test failed\n");
    return 0;
}