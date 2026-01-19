#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <signal.h>

#include "malloc369.h"
#include "sim.h"
#include "coremap.h"
#include "swap.h"
#include "tlb.h"
#include "multiprocessing.h"
#include "types.h"
#include "timer.h"
#include "parse_trace.h"

/* Counters for paging-related events. Set in pagetable.c */
extern size_t ram_hit_count;
extern size_t ram_miss_count;
extern size_t ref_count;
extern size_t evict_clean_count;
extern size_t evict_dirty_count;
extern size_t evict_dirty_count;
extern size_t cow_fault_count;
extern size_t write_fault_count;

// Define global variables declared in sim.h
size_t memsize = 0;
i32 debug = 0;
u8 *physmem = NULL;
struct frame *coremap = NULL;

/* Each eviction algorithm is represented by a structure with its name
 * and three functions.
 */
struct functions {
	const char *name;          // String name of eviction algorithm
	void (*init)();            // Initialize any data needed by alg
	void (*cleanup)();         // Cleanup any data initialized in init()
	void (*ref)(pfn_t);          // Called on each reference
	pfn_t (*evict)();            // Called to choose victim for eviction
};

/* The algs array gives us a mapping between the name of an eviction
 * algorithm as given in a command line argument, and the function to
 * call to select the victim page.
 *
 * The list of REPLACEMENT_ALGORITHMS is found in coremap.h
 * We use the C preprocessor stringizing and concatenation operations to
 * create a template for the algorithm function structure.
 * See https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html
 * and https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html
 */
static struct functions algs[] = {
#define RA(name) \
	{ #name, name ## _init, name ## _cleanup, name ## _ref, name ## _evict },
REPLACEMENT_ALGORITHMS
#undef RA
};
static i32 num_algs = sizeof(algs) / sizeof(algs[0]);

static void (*init_func)() = NULL;
static void (*cleanup_func)() = NULL;
void (*ref_func)(pfn_t) = NULL;
pfn_t (*evict_func)() = NULL;

/* An actual memory access based on the vaddr from the trace file.
 *
 * The find_physpage() function is called to translate the virtual address
 * to a (simulated) physical address -- that is, a pointer to the right
 * location in physmem array. The find_physpage() function is responsible for
 * everything to do with memory management - including translation using the
 * pagetable, allocating a frame of (simulated) physical memory (if needed),
 * evicting an existing page from the frame (if needed) and reading the page
 * in from swap (if needed).
 *
 * We then check that the memory has the expected content (just a copy of the
 * virtual address) and, in case of a write reference, increment the version
 * counter.
 */
static void
access_mem(char type, vaddr_t vaddr, u8 val, size_t linenum)
{
	u8 *memptr;
	const off_t offset = vaddr % PAGE_SIZE;
	const asid_t asid = current_task_id();
	pagetable_t *const pt = get_pagetable(current_task()->mm);

	paddr_t memaddr = tlb_translate(type, asid, pt, vaddr);
	pfn_t frame = memaddr >> PAGE_SHIFT;
	memptr = &physmem[frame * SIMPAGESIZE] + offset;

	if ((type == 'S') || (type == 'M')) {
		// write access to page, update value in simulated memory
		*memptr = val;
	} else if ((type == 'L' || type == 'I')) {
		if (*memptr != val) {
			printf("ERROR at trace line %zu: vaddr has %hhu but should have %hhu\n",
			       linenum, *memptr, val);
		}
	}
}

static void
replay_trace()
{
	struct trace_line tl;
	size_t linenum = 0;
	u32 curtask_i = 0;
	while (get_traceline(&tl)) {
		++linenum;

		if (strchr("ILSMBEF", tl.reftype) == NULL) {
			fprintf(stderr,"Invalid reftype, line %zu: reftype=%c\n",
				linenum, tl.reftype);
			exit(1);
		}
		if (strchr("ILSM", tl.reftype) != NULL
			&& (tl.vaddr % PAGE_SIZE) > SIMPAGESIZE) {
			fprintf(stderr,"Invalid vaddr, offset must be in range of simulated page frame size, line %zu: vaddr=%zu\n",
				linenum, tl.vaddr);
			exit(1);
		}
		if (tl.reftype == 'B') {
			create_task(tl.vpid);
			continue;
		}
		if (tl.reftype == 'E') {
			free_task(get_task_by_id(tl.vpid));
			continue;
		}

		if (debug >=  1) {
			printf("%u %c %lx %hhu\n", tl.vpid, tl.reftype, tl.vaddr, tl.value);
		}
		
		if (current_task() == NULL || curtask_i != tl.vpid) {
			task_switch(get_task_by_id(tl.vpid));
			curtask_i = tl.vpid;
		}
		if (tl.reftype == 'F') {
			fork369(current_task_id(), tl.vaddr);
			continue;
		}
		access_mem(tl.reftype, tl.vaddr, tl.value, linenum);
	}
}

void
usage(char *prog)
{
	fprintf(stderr,
		"USAGE: %s -f tracefile "
		"-m memorysize -s swapsize -a algorithm -t tlbsize [-d num]\n", prog);
	fprintf(stderr, "\t-f tracefile  - path to trace file to simulate\n");
	fprintf(stderr, "\t-m memorysize - number of physical memory frames\n");
	fprintf(stderr, "\t-s swapsize   - number of frames in swapfile\n");
	fprintf(stderr, "\t-a algorithm  - replacement algorithm to use, one of:\n");
	for (i32 i = 0; i < num_algs; ++i) {
		fprintf(stderr, "\t\t%s\n",algs[i].name);
	}
	fprintf(stderr, "\t-t tlbsize    - number of tlb entries (1-255, default 64)\n");
	fprintf(stderr, "\t-d num        - debug level for output\n");
}

i32
main(i32 argc, char *argv[])
{
	f64 starttime;
	f64 endtime;
	i64 start_mallocs;
	i64 start_bytes;
	i64 bytes_used;
	size_t swapsize = 0;
	char *tracefile = NULL;
	char *replacement_alg = NULL;
	i32 opt;

	struct mp_config mp_cfg = { .max_nr_tasks = -1 };
	struct tlb_config tlb_cfg = {
	    .seed = 369,
	};
	
	while ((opt = getopt(argc, argv, "f:m:a:s:d:t:h")) != -1) {
		switch (opt) {
		case 'f':
			tracefile = optarg;
			break;
		case 'm':
			memsize = strtoul(optarg, NULL, 10);
			break;
		case 'a':
			replacement_alg = optarg;
			break;
		case 's':
			swapsize = strtoul(optarg, NULL, 10);
			break;
		case 'd':
			debug = strtol(optarg, NULL, 10);
			break;
		case 't': {
			u64 tmp = strtoul(optarg, NULL, 10);
			if (tmp > TLB_MAXIMUM_SIZE) {
				fprintf(stderr, "Maximum TLB size 255 is exceeded.\n");
				return 1;
			}

			tlb_cfg.size = tmp;
			break;
		}
		case 'h':
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (!tracefile || !memsize || !swapsize || !replacement_alg) {
		usage(argv[0]);
		return 1;
	}

	// Initialize the page replacement algorithm function pointers
	for (i32 i = 0; i < num_algs; ++i) {
		if (strcmp(algs[i].name, replacement_alg) == 0) {
			init_func = algs[i].init;
			cleanup_func = algs[i].cleanup;
			ref_func = algs[i].ref;
			evict_func = algs[i].evict;
			break;
		}
	}

	if (!evict_func) {
		fprintf(stderr, "Error: invalid replacement algorithm - %s\n",
				replacement_alg);
		return 1;
	}
	
	// Initialize main data structures for simulation.
	// This happens before calling the replacement algorithm init function
	// so that the init_func can refer to the coremap if needed.
	init_soft_tlb(&tlb_cfg);
	init_csc369_malloc(false);

	// Get initial memory usage after malloc library is initialized
	start_mallocs = get_current_num_mallocs();
	start_bytes = get_current_bytes_malloced();

	init_coremap();
	physmem = malloc369(memsize * SIMPAGESIZE);
	memset(physmem, 0, memsize*SIMPAGESIZE);
	swap_init(swapsize);

	// Timed section of code starts here. This includes:
	//     - initialization of the multiprocessing code
	//     - initialization of the replacement algorithm
	//     - replaying the trace
	starttime = get_time();

	init_multiprocessing(&mp_cfg);
	init_func();      /* replacement algorithm initialization */
	init_parse_trace(tracefile);
	replay_trace();

	endtime = get_time();
	// End of timed section of code.

	// Get final memory use.
	bytes_used = get_current_bytes_malloced() - start_bytes;
	
	// Print statistics.
	size_t access_count = tlb_hit_count() + tlb_miss_count();
	printf("TLB Hit count: %zu\n", tlb_hit_count());
	printf("TLB Miss count: %zu\n", tlb_miss_count());
	printf("Memory Access count: %zu\n", access_count);
	printf("RAM Hit count: %zu\n", ram_hit_count);
	printf("RAM Miss count: %zu\n", ram_miss_count);
	printf("CoW Fault count: %zu\n", cow_fault_count);
	printf("Write Fault count: %zu\n", write_fault_count);
	printf("Clean evictions: %zu\n", evict_clean_count);
	printf("Dirty evictions: %zu\n", evict_dirty_count);
	printf("Swap In count: %zu\n", swap_pagein_count());
	printf("Swap Out count: %zu\n", swap_pageout_count());
	printf("Total references: %zu\n", ref_count);
	printf("TLB Hit rate: %.4f\n", ((f64)tlb_hit_count() / access_count) * 100.0);
	printf("TLB Miss rate: %.4f\n", ((f64)tlb_miss_count() / access_count) * 100.0);
	printf("RAM Hit rate: %.4f\n", ((f64)ram_hit_count / ref_count) * 100.0);
	printf("RAM Miss rate: %.4f\n", ((f64)ram_miss_count / ref_count) * 100.0);

	printf("Time to run simulation: %f\n",endtime - starttime);
	printf("Memory used by simulation: %ld bytes\n", bytes_used);
	
	cleanup_func();

	// Cleanup data structures and remove temporary swapfile
	// fclose(tfp);
	destroy_coremap();
	free369(physmem);
	swap_destroy();
	free_multiprocessing();

	// Check for memory leaks
	if (is_leak_free(start_mallocs, start_bytes)) {
		printf("No memory leaks detected.\n");
	} else {
		i64 bytes_leaked = get_current_bytes_malloced();
		i64 unfreed_mallocs = get_current_num_mallocs();
		printf("Detected %lu bytes leaked from %lu un-freed mallocs.\n",
		       bytes_leaked, unfreed_mallocs);
	}
	
	destroy_csc369_malloc();
	destroy_soft_tlb();
	return 0;
}
