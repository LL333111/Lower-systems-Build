/** @file coremap.h
 *
 * This file contains definitions that are needed to manage physical frames
 * of memory. Everything in this file should be independent of the page table
 * format.
 * 
 * @author Angela Brown
 * @author Kuei (Jack) Sun
 * @author Nagata Parama Aptana
 */

#ifndef __COREMAP_H__
#define __COREMAP_H__

#include "types.h"
#include "pagetable.h"
#include "list.h"
#include "ptrarray.h"

typedef struct frame frame_t;

/* The coremap holds information about physical memory.
 * The index into coremap is the physical page frame number stored
 * in the page table entry (pt_entry_t).
 */
extern frame_t *coremap;

// Coremap functions used in sim.c for initialization and teardown.
void init_coremap(void);
void destroy_coremap(void);

/**
 * @brief Allocates a frame to be used for the virtual page represented by pte.
 * If all frames are in use, calls the replacement algorithm's evict_func to
 * select a victim frame. Writes victim to swap if needed, and updates
 * page table entry for victim to indicate that virtual page is no longer in
 * (simulated) physical memory.
 * 
 * @param pte[in] Pointer to the page table entry representing the virtual page.
 * @return The physical frame number allocated for the virtual page, guaranteed
 * to be valid.
 *
 * @see coremap.c
 */
pfn_t allocate_frame(pt_entry_t *pte);
void init_frame(pfn_t frame);

/**
 * @brief Get pointer to frame object referring to the given frame number.
 * 
 * @param framenum[in] Frame number to get.
 * @return Pointer to frame object that represents the `framenum`-th physical
 * frame, or null pointer if there is no such physical frame.
 * 
 * @see coremap.c
 */
frame_t *frame_from_number(pfn_t framenum);
frame_t *frame_from_list_entry(list_entry *entry);
pfn_t get_frame_number(const frame_t *pframe);
list_entry *get_frame_list_entry(frame_t *pframe);
bool frame_in_use(const frame_t *pframe);
bool frame_is_shared(const frame_t *pframe);

/**
 * @brief Link a page table entry to a physical frame.
 *
 * @param framenum[in] The physical frame number to link the PTE to.
 * @param pte[in] The pointer referring to the page table entry to link.
 * 
 * @see coremap.c
 */
void frame_link_pte(pfn_t framenum, pt_entry_t *pte);

/**
 * @brief Unlink a page table entry from a physical frame.
 *
 * @param framenum[in] The physical frame number to unlink the PTE from.
 * @param pte[in] The pointer referring to the page table entry to remove.
 * 
 * @see coremap.c
 */
void frame_unlink_pte(pfn_t framenum, pt_entry_t *pte);

// Accessor functions for coremap, for pagetable specific handling
// logic that you need to implement

/**
 * @brief Evict the frame from physical memory to swap.
 *
 * Called from allocate_frame() in coremap.c after a victim page frame has
 * been selected.
 * 
 * Only write to swap if necessary. Counters for evictions should be updated
 * appropriately in this function.
 * 
 * @param framenum[in] Frame number that will get evicted.
 * @param asid[in] The address space identifier of the would-be evicted frame.
 * 
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
void handle_frame_evict(pfn_t framenum, asid_t asid);

// Accessor functions for page table entries, to allow replacement
// algorithms to obtain information from a PTE, without depending
// on the internal implementation of the structure.

bool get_referenced(const frame_t *frame);
void set_referenced(frame_t *frame, bool val);

/**
 * @brief Get all page table entries that refer to a given frame.
 *
 * @param frame[in] The pointer to frame in question.
 * @return A slice of page table entries that refer to the given frame.
 *
 * @see coremap.c
 */
ptrarray_slice_t __nonnull() get_referring_ptes(const frame_t *frame);

// The replacement algorithms.
#define REPLACEMENT_ALGORITHMS \
	RA(rand) \
	RA(rr) \
	RA(clock) \
	RA(s2q) 
// no longer part of the assignment: lru, mru, opt

// Replacement algorithm functions.
// These may not need to do anything for some algorithms.
#define RA(name) \
	void name ## _init(); \
	void name ## _cleanup(); \
	void name ## _ref(pfn_t); \
	pfn_t name ## _evict();
REPLACEMENT_ALGORITHMS
#undef RA

#endif /* __COREMAP_H__ */
