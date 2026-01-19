#ifndef _TIMEOUT_H_
#define _TIMEOUT_H_

/* Credits to this stack overflow post: 
 * 
 * https://stackoverflow.com/questions/282176/waitpid-equivalent-with-timeout 
 * 
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/select.h>
#include <stdio.h>

static int selfpipe[2];

static void 
selfpipe_sigh(int n)
{
    int save_errno = errno;
    (void)write(selfpipe[1], &n, 1);
    errno = save_errno;
    (void)n;
}

static void
selfpipe_setup(void)
{
    static struct sigaction act;
    if (pipe(selfpipe) == -1) 
        abort();

    fcntl(selfpipe[0], F_SETFL, fcntl(selfpipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(selfpipe[1], F_SETFL, fcntl(selfpipe[1], F_GETFL) | O_NONBLOCK);
    memset(&act, 0, sizeof(act));
    act.sa_handler = selfpipe_sigh;
    sigaction(SIGCHLD, &act, NULL);
}

static int
selfpipe_waitpid(pid_t child_pid, int timeout_secs)
{
    char dummy;
    fd_set rfds;
    struct timeval start, elapsed;
    int st, ret;

    gettimeofday(&start, NULL);
    FD_ZERO(&rfds);
    
    while (1) {
        struct timeval end, tv, limit = { timeout_secs, 0 };
        struct timeval * tvp = NULL;

        gettimeofday(&end, NULL);
        timersub(&end, &start, &elapsed);
        
        if (elapsed.tv_sec >= timeout_secs) {
            ret = kill(child_pid, SIGKILL);
            assert(ret == 0);
        }
        else {
            timersub(&limit, &elapsed, &tv);
            tvp = &tv;
        }        

        FD_SET(selfpipe[0], &rfds);
        ret = select(selfpipe[0]+1, &rfds, NULL, NULL, tvp);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            st = 0;
            break;
        }

        if (FD_ISSET(selfpipe[0], &rfds)) {
            ret = read(selfpipe[0], &dummy, sizeof(dummy));
            assert(ret == 1);
            ret = waitpid(-1, &st, WNOHANG);
            if (ret == child_pid) {
                break;
            }
        }
    }

    return st;
}

// test case types
typedef int (* test_f)(void);
typedef struct _tc {
    const char * name; 
    test_f func; 
} testcase_t;

// forward declaration
int run_test_case(int test_id);
void wait_process(pid_t pid);
extern testcase_t test_case[];
extern int nr_cases;
extern const int timeout_secs;

static int
test_process(FILE * lf, int test_id)
{
    fprintf(lf, "Test Case %d: %s\n", test_id+1, test_case[test_id].name);
    fflush(lf);

    // write all console messages to log file
    dup2(fileno(lf), STDOUT_FILENO);
    dup2(fileno(lf), STDERR_FILENO);

    // turn off all unused fds
    close(selfpipe[0]);
    close(selfpipe[1]);
    
    return run_test_case(test_id);
}

static void 
usage(const char * prog)
{
    fprintf(stderr, "usage: %s [1..%d]\n", prog, nr_cases);
    exit(EXIT_FAILURE);
}

static int
main_process(const char * testname, int argc, const char * argv[])
{
    if (argc == 2) {
        int testnum = atoi(argv[1]);
        if (testnum <= 0 || testnum > nr_cases) {
            usage(argv[0]);
        }
        return run_test_case(testnum - 1);
    }
    else if (argc != 1) {
        usage(argv[0]);
    }

    char logname[128];
    sprintf(logname, "%s.log", testname);
    FILE * lf = fopen(logname, "wt");
    selfpipe_setup();
    printf("starting %s test\n", testname);

    for (int i = 0; i < nr_cases; i++) {
        pid_t pid;
        printf("Test Case %d: %s\n", i+1, test_case[i].name);
        fflush(stdout);
        pid = fork();
        if (pid == 0) {
            return test_process(lf, i);
        }
        else if (pid > 0) {
            wait_process(pid);
        } else {
            printf("ERROR: fork() failed!\n");
            exit(EXIT_FAILURE);
        }
    }

    fclose(lf);
    printf("%s test done\n", testname);
    return 0;
}

#endif // _TIMEOUT_H_