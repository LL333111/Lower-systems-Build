#include <stdlib.h>
#include "sim.h"
#include "coremap.h"
#include "types.h"

/**
 * @brief Select a page to evict using the RAND algorithm.
 *
 * @return The frame number (index in the coremap) of the page to evict.
 */
pfn_t rand_evict(void)
{
	//NOTE: We keep the default seed (don't call srandom) for repeatable results
	pfn_t result = INVALID_FRAME;
	frame_t *f = NULL;
	do {
		result = random() % memsize;
		f = frame_from_number(result);
	} while (frame_is_shared(f));

	return result;
}

/**
 * @brief Called on each access to a page to update any information
 * needed by the RAND algorithm.
 *
 * @param framenum[in] The frame number being accessed.
 */
void rand_ref(pfn_t framenum)
{
	(void)framenum;
}

/**
 * @brief Initialize data structures for the RAND algorithm.
 */
void rand_init(void)
{
}

/**
 * @brief Clean up data structures used by the RAND algorithm.
 */
void rand_cleanup(void)
{
}