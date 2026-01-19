#ifndef __SWAP_H__
#define __SWAP_H__

#include "types.h"

#define INVALID_SWAP (off_t)-1

// Swap functions for use in other files
extern void swap_init(size_t size);
extern void swap_destroy(void);

/**
 * @brief Read data into (simulated) physical memory `frame` from `offset` in
 * swap file.
 * 
 * @param frame[in] The physical frame number (not byte offset in physmem).
 * @param offset[in] The byte position in the swap file.
 * @return 0 on success,
 * -errno on error or number of bytes read on partial read.
 * 
 * @see swap.c
 */
extern i32 swap_pagein(pfn_t frame, off_t offset);

/**
 * @brief Write data from (simulated) physical memory `frame` to `offset` in
 * swap file. Allocates space in swap file for virtual page if needed.
 * 
 * @param frame[in] The physical frame number (not byte offset in physmem).
 * @param offset[in] The byte position in the swap file.
 * @return the offset where the data was written on success,
 * or INVALID_SWAP on failure.
 * 
 * @see swap.c
 */
extern off_t swap_pageout(pfn_t frame, off_t offset);

/**
 * @brief Free a swap space at the given offset.
 * 
 * @param offset[in] The byte position in the swap file.
 * 
 * @see swap.c
 */
extern void swap_free(off_t offset);

/**
 * @brief Return the number of page-in events thus far in the simulation.
 *
 * @return The number of page-in events.
 *
 * @see swap.c
 */
extern size_t swap_pagein_count(void);

/**
 * @brief Return the number of page-out events thus far in the simulation.
 *
 * @return The number of page-out events.
 *
 * @see swap.c
 */
extern size_t swap_pageout_count(void);


#endif /* __SWAP_H__ */
