#include "types.h"
#include "coremap.h"
#include "sim.h"
#include "malloc369.h"

#include <assert.h>
#include <string.h>

/**
 * @brief Select a page to evict using the CLOCK algorithm.
 *
 * @return The frame number (index in the coremap) of the page to evict.
 */
static pfn_t clock_c = 0;
pfn_t clock_evict(void)
{
	while (1)
	{
		if ((size_t)clock_c >= memsize)
		{
			clock_c = 0;
		}

		frame_t *frame = frame_from_number(clock_c);
		if (!get_referenced(frame))
		{
			pfn_t victim = clock_c;
			clock_c = (clock_c + 1) % (pfn_t)memsize;
			return victim;
		}

		set_referenced(frame, false);
		clock_c = (clock_c + 1) % (pfn_t)memsize;
	}
}

/**
 * @brief Called on each access to a page to update any information
 * needed by the CLOCK algorithm.
 *
 * @param framenum[in] The frame number being accessed.
 */
void clock_ref(pfn_t framenum)
{
	frame_t *frame = frame_from_number(framenum);
	if (frame == NULL)
	{
		return;
	}
	set_referenced(frame, true);
}

/**
 * @brief Initialize data structures for the CLOCK algorithm.
 */
void clock_init(void)
{
	clock_c = 0;
	for (pfn_t i = 0; (size_t)i < memsize; i++)
	{
		frame_t *frame = frame_from_number(i);
		if (frame != NULL)
		{
			set_referenced(frame, false);
		}
	}
}

/**
 * @brief Clean up data structures used by the CLOCK algorithm.
 */
void clock_cleanup(void)
{
	clock_c = 0;
}