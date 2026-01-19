#include "../ut369.h"
#include <assert.h>
#include <stdio.h>

int check_ran_new(void * arg)
{
    bool * vp = arg;
    *vp = true;
    return 0;
}

int check_ran_ready(void * arg)
{
    bool * vp = arg;

    printf("child thread %d yields.\n", thread_id());
    Tid ret = thread_yield(THREAD_ANY);
    assert(ret >= 0);
    *vp = true;

    while(true) {
        ret = thread_yield(THREAD_ANY);
        assert(ret >= 0);
    }

    return 0;
}

int kill_init(void * arg)
{
    Tid id = (long)arg;
    Tid ret = thread_kill(id);
    printf("kill(%d) = %d\n", id, ret);
    return 0;
}

int main() 
{
    struct config config = { 
        .sched_name = "rand", 
        .preemptive = false 
    };

    ut369_start(&config);
    
    // try a few error cases
    Tid ret = thread_kill(thread_id());
	printf("kill(SELF) = %d\n", ret);
    ret = thread_kill(42);
	printf("kill(NOTFOUND) = %d\n", ret);
	ret = thread_kill(-42);
	printf("kill(NEGATIVE) = %d\n", ret);
	ret = thread_kill(THREAD_MAX_THREADS + 1000);
	printf("kill(TOOBIG) = %d\n", ret);

    // kill a newly created thread before it runs
    bool ran = false;
    Tid tid = thread_create(check_ran_new, &ran);
    assert(tid >= 0);

    ret = thread_kill(tid);
    printf("kill(%d) = %d\n", tid, ret);

    ret = thread_kill(tid);
    printf("kill(KILLED) = %d\n", ret);

    ret = thread_yield(tid);
    assert(ret == tid);
    printf("did killed thread run before exit? %s\n", ran ? "yes" : "no");

    ret = thread_kill(tid);
    printf("kill(ZOMBIE) = %d\n", ret);
    // intentionally leaving zombie not cleaned up

    ran = false;
    tid = thread_create(check_ran_ready, &ran);
    assert(tid >= 0);
    
    // let it run once so its context is saved inside thread_switch
    ret = thread_yield(tid);
    assert(ret >= 0);

    ret = thread_kill(tid);
    printf("kill(%d) = %d\n", tid, ret);

    ret = thread_yield(tid);
    assert(ret == tid);
    printf("did killed thread run before exit? %s\n", ran ? "yes" : "no");

    // let helper thread kill the init thread
    tid = thread_create(kill_init, (void *)(long)thread_id());
    assert(tid >= 0);

    while(true)
        thread_yield(THREAD_ANY);

    assert(false);
    return 0;
}