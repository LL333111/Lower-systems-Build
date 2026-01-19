/** @file parse_trace.h
 * @brief Header-Only Implementation of Trace Parsing
 * 
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * @author Nagata Parama Aptana
 * @author Louis Ryan Tan
 *
 */
#ifndef __PARSE_TRACE_H__
#define __PARSE_TRACE_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>    // open
#include <sys/mman.h> // mmap
#include <sys/stat.h> // fstat

#include "types.h"

struct trace_line {
	u32 vpid;
	u8 reftype;
	u8 value;
	vaddr_t vaddr;
};

static int trace_fd;
static char* trace_data = NULL; // (will be) pointer to mmap-ed region of the trace file

/* trace data info */
static int rem_data;    // remaining data that hasn't been loaded by mmap
static int data_offset; // offset from the beginning of the file

/* trace current chunk info */
static int chunk_offset; // offset within the current chunk
static int chunk_size;


#define MB ((1 << 20L))

// compile with -DPT_CHUNKSIZE=... to try different chunk sizes
#ifndef PT_CHUNKSIZE
#define PT_CHUNKSIZE 2 * MB
#endif

static inline
int get_remaining_in_chunk()
{
	return chunk_size - chunk_offset;
}

static inline
int get_next_chunk_size()
{
	int res = rem_data > PT_CHUNKSIZE ? PT_CHUNKSIZE : rem_data;
	return res;
}

static inline
bool have_data()
{
	return rem_data != 0;
}

/**
 * Maps at most PT_CHUNKSIZE bytes from the trace file starting from f_offset
 * into memory.
 */
static inline
void next_chunk()
{
	chunk_offset = 0; // reset offset
	int toread = get_next_chunk_size();
	trace_data = mmap(trace_data, toread,
			  PROT_READ, MAP_SHARED | MAP_POPULATE, trace_fd, data_offset);
	if (trace_data == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}
	chunk_size = toread;
	data_offset += toread;
	rem_data -= toread;
}

static inline
void init_parse_trace(const char *fname)
{
	trace_fd = open(fname, O_RDONLY);
	struct stat trace_sb;
	if (fstat(trace_fd, &trace_sb)) {
		perror("fstat");
		exit(1);
	}
	rem_data = trace_sb.st_size;
	data_offset = 0;
	trace_data = NULL;
	next_chunk();
}

static inline
void destroy_parse_trace()
{
	assert(munmap(trace_data, chunk_size) == 0);
	close(trace_fd);
}

/**
 * Reads a trace line into 'out'.
 * Returns true if there is still any trace line to read else returns false.
 * On false return, the value in 'out' may be undefined.
*/
static inline
bool get_traceline(struct trace_line * out)
{
	int want = sizeof(*out);
	int have = get_remaining_in_chunk();
	if (want > have) {
		char * outbytes = (char *) out;
		// not sufficient data
		if (!have_data()) {
			return false;
		}
		// read what you have
		memcpy(outbytes, trace_data + chunk_offset, have);
		outbytes += have;
		want -= have;

		next_chunk();
		assert(want < get_remaining_in_chunk());
		// read what you need left
		memcpy(outbytes, trace_data + chunk_offset, want);
	}
	memcpy(out, trace_data + chunk_offset, sizeof(*out));
	chunk_offset += sizeof(*out);
	return true;
}

#endif
