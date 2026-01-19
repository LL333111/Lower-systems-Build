/*
 * Contains basic data types that will be used in the simulation, defines 
 * configuration variables, memory size, and various data structure inits.
 */

#ifndef __SIM_H__
#define __SIM_H__

#include "tlb.h"
#include "types.h"

// utils
#define cdiv(x, y) (((x) + (y) - 1) / (y)) // ceil (x / y)

// constants
#define PAGE_SHIFT 12

#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE - 1))

#define SIMPAGESIZE 16         /* Simulated physical memory page frame size */

extern u8 *physmem;			   /* Array of bytes to simulate physical memory */
extern size_t memsize;         /* Number of frames of physical memory */
extern i32 debug;              /* Control amount of debugging output */

/* 
 *Configuration structs 
 */

// multiprocessing
struct mp_config {
	i32 max_nr_tasks;
};

// tlb
struct tlb_config {
	unsigned int seed;
	tlb_index_t size;
};

struct task_s;
struct pagetable;

/* Interface to multiprocessing functions that are called from sim.c */
extern i64 task_switch(struct task_s *newtask);

/**
 * @brief Load relevant address to TLB.
 *
 * Called from access_mem() in sim.c if tlb_resolve_addr() results in a fault.
 * 
 * @param asid[in] The address space identifier of the memory access.
 * @param pt[in] The pagetable of the address space.
 * @param vaddr[in] The virtual address to access.
 * @param type[in] The type of memory access.
 * @param write[in] `true` if it is a write fault, `false` otherwise.
 * 
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
extern void handle_tlb_fault(asid_t asid, struct pagetable * pt, vaddr_t vaddr,
			     char type, bool write);

/* Pointers to per-eviction algorithm functions needed in pagetable.c */

/**
 * @brief Reference a physical frame.
 *
 * This function must be called whenever a physical frame is accessed. The
 * information tracked by this function is used by the page replacement
 * algorithm to make decisions about which pages to evict.
 * 
 * The actual implementation depends on the command line argument given to
 * the `sim` executable.
 * 
 * @param framenum[in] The physical frame number being referenced.
 *
 * @see sim.c, clock.c, rand.c, rr.c, s2q.c
 */
extern void (*ref_func)(pfn_t framenum);

/**
 * @brief Select a physical frame to evict.
 *
 * This function determines which physical frame should be replaced according
 * to the active page replacement algorithm.
 *
 * The specific implementation depends on the command line argument given to
 * the `sim` executable.
 *
 * @return The frame number (index in the coremap) of the page to evict.
 *
 * @see sim.c, clock.c, rand.c, rr.c, s2q.c
 */
extern pfn_t (*evict_func)(void);

#endif /* __SIM_H__ */
