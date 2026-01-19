#include "../ut369.h"
#include <assert.h>
#include <stdio.h>

static inline int
thread_ret_ok(Tid ret)
{
	return (ret >= 0 ? 1 : 0);
}

int hello_repeat(void * arg)
{
    while (true) {
        fprintf(stderr, "hello: hi again\n");
        thread_yield(THREAD_ANY);
    }

    (void)arg;
    return 0;
}

int hello_once(void * arg)
{
    fprintf(stderr, "hello: sayonara\n");
    (void)arg;
    return 0;
}

int patricide(void * arg)
{
    Tid parent_tid = (long)arg;
    Tid ret = thread_kill(parent_tid);
    assert(ret == parent_tid);

    /* let the parent exit */
    ret = thread_yield(parent_tid);
    assert(ret == parent_tid);

    ret = thread_wait(parent_tid, NULL);
    assert(ret == 0);

    // should yield to init thread
    ret = thread_yield(THREAD_ANY);
    assert(thread_ret_ok(ret));
    
    return 0;
}

void victim(void * arg)
{
    Tid tid = thread_create(patricide, (void *)(long)thread_id());
    assert(thread_ret_ok(tid));

    while(1) {
        Tid ret = thread_yield(tid);
        assert(ret == tid);
    }

    (void)arg;
}

int main() 
{
    struct config config = { 
        .sched_name = "rand", 
        .preemptive = false 
    };
   
    ut369_start(&config);

    Tid ret;
    Tid tid = thread_create(hello_repeat, NULL);
    assert(thread_ret_ok(tid));

    ret = thread_yield(tid);
    printf("repeat: yield(%d) = %d\n", tid, ret);
    
    ret = thread_yield(THREAD_ANY);
    printf("repeat: yield(ANY) = %d\n", ret);

    ret = thread_kill(tid);
    assert(ret == tid);
    ret = thread_yield(THREAD_ANY);
    printf("repeat: yield(ANY+KILLED) = %d\n", ret);

    ret = thread_yield(THREAD_ANY);
    printf("repeat: yield(ANY+ZOMBIE) = %d\n", ret);

    Tid tid2 = thread_create(hello_once, NULL);
    assert(thread_ret_ok(tid2));
    assert(tid != tid2);

    ret = thread_yield(THREAD_ANY);
    printf("once: yield(ANY) = %d\n", ret);

    ret = thread_wait(tid, NULL);
    assert(ret == 0);

    ret = thread_yield(THREAD_ANY);
    printf("repeat: yield(ANY+FREED+ZOMBIE) = %d\n", ret);

    ret = thread_yield(tid);
    printf("repeat: yield(%d) = %d\n", tid, ret);

    ret = thread_yield(-42);
    printf("main: yield(NEGATIVE) = %d\n", ret);

    ret = thread_yield(THREAD_MAX_THREADS);
    printf("main: yield(TOOBIG) = %d\n", ret);

    ret = thread_yield((tid2 + 257) % THREAD_MAX_THREADS);
    printf("main: yield(NOTFOUND) = %d\n", ret);

    ret = thread_yield(thread_id());
    printf("main: yield(%d) = %d\n", thread_id(), ret);

    // test thread_yield where specified thread is freed by the time
    // the yielder returns.
    tid = thread_create((thread_entry_f)victim, NULL);
    assert(thread_ret_ok(tid));

    ret = thread_yield(tid);
    printf("victim: yield(%d) = %d\n", tid, ret);

    ret = thread_yield(tid);
    printf("victim: yield(%d) = %d\n", tid, ret);

    ret = thread_yield(THREAD_ANY);
    printf("victim: yield(ANY) = %d\n", ret);

    thread_exit(0);
    assert(false);
    return 0;
}