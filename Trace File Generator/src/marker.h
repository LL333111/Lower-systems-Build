/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Author: Louis Ryan Tan
 */

#ifndef __MARKER_H__
#define __MARKER_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Markers for Memory Tracing */

/* This file defines marking utility for CSC369 memory access tracing. The underlying
 * tracing tool used is Valgrind's Lackey.
 *
 * The mechanism for tracing is by accessing fixed locations in memory, 'marker's,
 * whose exact address is written out to disk. Since Lackey dumps 'all' accesses
 * (including setup calls into libc, but not syscalls), the accesses done by the program
 * itself has to be sandwiched between a 'start' and an 'end' marker. This file provides
 * marker_start(outfile), marker_end(), and fork369() to detect fork syscalls.
 *
 */

/* ------- Static Markers --------- */
static volatile char __start_marker = 0;
static volatile char __end_marker = 0;

static volatile char __fork369_start = 0;
static volatile char __fork369_end = 0;

static volatile char __fork369_fork_start = 0;
static volatile char __fork369_fork_end = 0;

static volatile char __is_parent;

/* ------ Private variable ------- */
static FILE * __fout;

// memory barrier to prevent load-store reordering by the compiler
// works by 'clobbering' memory, telling the compiler that the memory 'might'
// have been tampered with, so it cannot guarantee reodering has no side effects
#define MEMBARRIER() asm volatile("" ::: "memory")

#define __MARKER_START() \
do { \
	MEMBARRIER(); \
	__start_marker = 'S'; \
} while(0);

#define __MARKER_END() \
do { \
	MEMBARRIER(); \
	__end_marker = 'E'; \
} while(0);

static __always_inline void marker_start(const char *path)
{
	__fout = fopen(path, "w");
	if (__fout == NULL) {
		perror(path);
		exit(1);
	}
	/* do line buffering to reduce redundant fflush calls this is necessary
	 * because children will copy its parent's buffers, so the output file's
	 * content could have redundancies if not flushed appropriately
	 **/
	setlinebuf(__fout);

	fprintf(__fout, "%i %p %p %p %p %p %p %p\n",
		getpid(),
		&__start_marker, &__end_marker,
		&__fork369_start, &__fork369_end,
		&__fork369_fork_start, &__fork369_fork_end,
		&__is_parent
	);
	__MARKER_START();
}

static __always_inline void marker_end()
{
	__MARKER_END();
	fclose(__fout);
	__fout = NULL;
}

#define __MARK_FORK369_START()	\
	do {						\
		MEMBARRIER();			\
		__fork369_start = 'f';	\
	} while (0);

#define __MARK_FORK369_END()	\
	do {						\
		MEMBARRIER();			\
		__fork369_end = 'F';	\
	} while (0);

#define __MARK_FORK369_FORK_START()	\
	do {							\
		MEMBARRIER();				\
		__fork369_fork_start = 'f';	\
	} while (0);

#define __MARK_FORK369_FORK_END()	\
	do {							\
		MEMBARRIER();				\
		__fork369_fork_end = 'F';	\
	} while (0);

#define __MARK_AS_PARENT()	\
	do {					\
		MEMBARRIER();		\
		__is_parent = 'y';	\
	} while (0);

static __always_inline void register_child_pid(pid_t cpid)
{
	MEMBARRIER();
	if (cpid == 0) {
		return;
	}
	__MARK_AS_PARENT();
	fprintf(__fout, "%i=>%i\n", getpid(), cpid);
}

/**
 * Wrapper around fork(2), this is used to detect that a child process has been created
 * and keeps track of its pid. This is necessary for multiprocess memory tracing.
 */
static __always_inline pid_t fork369()
{
	__MARK_FORK369_START();
	__MARK_FORK369_FORK_START();
	const pid_t cpid = fork();
	__MARK_FORK369_FORK_END();
	register_child_pid(cpid);
	__MARK_FORK369_END();
	return cpid;
}

#undef __MARK_FORK369_START
#undef __MARK_FORK369_END
#undef __MARK_FORK369_FORK_START
#undef __MARK_FORK369_FORK_END

#undef MEMBARRIER

#endif /* __MARKER_H__ */
