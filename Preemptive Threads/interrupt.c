#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include "ut369.h"
#include "interrupt.h"

static void interrupt_handler(int sig, siginfo_t *sip, void *contextVP);
static void set_interrupt(void);
static void set_signal(sigset_t *setp);

static int init = 0;
static int loud = 0;

/* Called as part of ut369_start. Many of the calls won't
 * make sense at first -- study the man pages!
 */
void interrupt_init(int verbose)
{
	struct sigaction action;
	int error;

	assert(!init); /* should only register once */
	init = 1;
	loud = verbose;
	action.sa_handler = NULL;
	action.sa_sigaction = interrupt_handler;
	error = sigemptyset(&action.sa_mask);
	assert(!error);

	/* use sa_sigaction as handler instead of sa_handler */
	action.sa_flags = SA_SIGINFO;
	if (sigaction(SIG_TYPE, &action, NULL))
	{
		perror("Setting up signal handler");
		assert(0);
	}

	/* block SIG_TYPE while interrupt_handler() is running. This will avoid
	 * recursive interrupts where an interrupt occurs before the previous
	 * interrupt handler has finished running.  */
	interrupt_off();
	set_interrupt();
}

void interrupt_end(void)
{
	/* ignore all subsequent signals */
	signal(SIG_TYPE, SIG_IGN);
	init = 0;
}

/* enables interrupts. */
int interrupt_on(void)
{
	return interrupt_set(1);
}

/* disables interrupts */
int interrupt_off(void)
{
	return interrupt_set(0);
}

/* enables or disables interrupts, and returns whether interrupts were enabled
 * or not previously. */
int interrupt_set(int enabled)
{
	int ret;
	sigset_t mask, omask;

	set_signal(&mask);
	if (enabled)
	{
		ret = sigprocmask(SIG_UNBLOCK, &mask, &omask);
	}
	else
	{
		ret = sigprocmask(SIG_BLOCK, &mask, &omask);
	}
	assert(!ret);
	return (sigismember(&omask, SIG_TYPE) ? 0 : 1);
}

int interrupt_enabled(void)
{
	sigset_t mask;
	int ret;

	if (!init)
		return 0;

	ret = sigprocmask(0, NULL, &mask);
	assert(!ret);
	return (sigismember(&mask, SIG_TYPE) ? 0 : 1);
}

void interrupt_quiet(void)
{
	loud = 0;
}

void spin(int usecs)
{
	struct timeval start, end, diff;
	int ret;

	ret = gettimeofday(&start, NULL);
	assert(!ret);
	while (1)
	{
		ret = gettimeofday(&end, NULL);
		timersub(&end, &start, &diff);

		if ((diff.tv_sec * 1000000 + diff.tv_usec) >= usecs)
		{
			break;
		}
	}
}

/* turn off interrupts while printing */
int unintr_printf(const char *fmt, ...)
{
	int ret, enabled;
	va_list args;

	enabled = interrupt_off();
	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);
	interrupt_set(enabled);
	return ret;
}

/* static functions */

static void
set_signal(sigset_t *setp)
{
	int ret;
	ret = sigemptyset(setp);
	assert(!ret);
	ret = sigaddset(setp, SIG_TYPE);
	assert(!ret);
	return;
}

static int first = 1;
static struct timeval start, end, diff = {0, 0};

/*
 * STUB: once interrupt_init() is called, this routine
 * gets called each time SIG_TYPE is sent to this process
 */
static void
interrupt_handler(int sig, siginfo_t *sip, void *contextVP)
{
	ucontext_t *context = (ucontext_t *)contextVP;
	(void)sig;
	(void)sip;

	/* check that SIG_TYPE is blocked on entry.
	 * this signal should be blocked because of the sigemptyset call in
	 * register_interrupt_handler(). */
	assert(!interrupt_enabled());
	if (loud)
	{
		int ret;
		ret = gettimeofday(&end, NULL);
		assert(!ret);
		if (first)
		{
			first = 0;
		}
		else
		{
			timersub(&end, &start, &diff);
		}
		start = end;
		printf("%s: context at %10p, time diff = %ld us\n",
			   __FUNCTION__, context,
			   diff.tv_sec * 1000000 + diff.tv_usec);
	}

	set_interrupt();
	/* implement preemptive threading by calling thread_yield */
	thread_yield(THREAD_ANY);
}

/*
 * Use the setitimer() system call to set an alarm in the future. At that time,
 * this process will receive a SIGALRM signal.
 */
static void
set_interrupt(void)
{
	int ret;
	struct itimerval val;

	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = 0;

	val.it_value.tv_sec = 0;
	val.it_value.tv_usec = SIG_INTERVAL;

	ret = setitimer(ITIMER_REAL, &val, NULL);
	assert(!ret);
}
