#include "sim.h"
#include "coremap.h"
#include "types.h"

/**
 * @brief Select a page to evict using the Round Robin algorithm.
 *
 * Equivalent to FIFO for single-process traces. In multiprocess scenarios,
 * shared frames are skipped and never selected for eviction.
 *
 * @return The frame number (index in the coremap) of the page to evict.
 */
pfn_t rr_evict(void)
{
	static pfn_t i = 0;
	pfn_t victim = INVALID_FRAME;

	for (size_t count = 0; count < memsize; count += 1, i = (i + 1) % memsize) {
		frame_t *fi = frame_from_number(i);
		if (!frame_is_shared(fi)) {
			victim = i;
			i = (i + 1) % memsize;
			break;
		}
	}

	return victim;
}

/**
 * @brief Update Round Robin state on a page access.
 *
 * @param framenum[in] The frame number being accessed.
 */
void rr_ref(pfn_t framenum)
{
	(void)framenum;
}

/**
 * @brief Initialize data structures for the Round Robin algorithm.
 */
void rr_init(void)
{
}

/**
 * @brief Clean up data structures used by the Round Robin algorithm.
 */
void rr_cleanup(void)
{
}