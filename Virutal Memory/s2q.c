#include "types.h"
#include "coremap.h"
#include "sim.h"
#include "malloc369.h"

#include <assert.h>
#include <string.h>

extern size_t memsize;

typedef enum
{
	S2Q_STATE_NONE = 0,
	S2Q_STATE_A1 = 1,
	S2Q_STATE_A2 = 2
} s2q_state_t;

static s2q_state_t *s2q_states = NULL;
static pfn_t *s2q_next = NULL;
static pfn_t *s2q_prev = NULL;
static pfn_t a1_head = INVALID_FRAME;
static pfn_t a1_tail = INVALID_FRAME;
static pfn_t a2_head = INVALID_FRAME;
static pfn_t a2_tail = INVALID_FRAME;

static size_t a1_size = 0;
static size_t a2_size = 0;
static size_t a1_threshold = 0;

static void
queue_push_back(pfn_t *head, pfn_t *tail, pfn_t f)
{
	if (*head == INVALID_FRAME)
	{
		*head = *tail = f;
		s2q_prev[f] = INVALID_FRAME;
		s2q_next[f] = INVALID_FRAME;
	}
	else
	{
		s2q_prev[f] = *tail;
		s2q_next[f] = INVALID_FRAME;
		s2q_next[*tail] = f;
		*tail = f;
	}
}

static pfn_t
queue_pop_front(pfn_t *head, pfn_t *tail)
{
	if (*head == INVALID_FRAME)
	{
		return INVALID_FRAME;
	}

	pfn_t f = *head;
	pfn_t n = s2q_next[f];

	if (n != INVALID_FRAME)
	{
		s2q_prev[n] = INVALID_FRAME;
		*head = n;
	}
	else
	{
		*head = *tail = INVALID_FRAME;
	}

	s2q_next[f] = s2q_prev[f] = INVALID_FRAME;
	return f;
}

static void
queue_remove(pfn_t *head, pfn_t *tail, pfn_t f)
{
	pfn_t p = s2q_prev[f];
	pfn_t n = s2q_next[f];

	if (p != INVALID_FRAME)
	{
		s2q_next[p] = n;
	}
	else
	{
		*head = n;
	}

	if (n != INVALID_FRAME)
	{
		s2q_prev[n] = p;
	}
	else
	{
		*tail = p;
	}

	s2q_next[f] = s2q_prev[f] = INVALID_FRAME;
}

static void
a1_insert_back(pfn_t f)
{
	queue_push_back(&a1_head, &a1_tail, f);
	a1_size++;
}

static pfn_t
a1_pop_front(void)
{
	pfn_t f = queue_pop_front(&a1_head, &a1_tail);
	if (f != INVALID_FRAME)
	{
		a1_size--;
	}
	return f;
}

static void
a1_remove(pfn_t f)
{
	queue_remove(&a1_head, &a1_tail, f);
	a1_size--;
}

static void
a2_insert_back(pfn_t f)
{
	queue_push_back(&a2_head, &a2_tail, f);
	a2_size++;
}

static pfn_t
a2_pop_front(void)
{
	pfn_t f = queue_pop_front(&a2_head, &a2_tail);
	if (f != INVALID_FRAME)
	{
		a2_size--;
	}
	return f;
}

static void
a2_remove(pfn_t f)
{
	queue_remove(&a2_head, &a2_tail, f);
	a2_size--;
}

/**
 * @brief Select a page to evict using the simplified 2Q algorithm.
 *
 * @return The frame number (index in the coremap) of the page to evict.
 */
pfn_t s2q_evict(void)
{
	pfn_t victim = INVALID_FRAME;
	if (a1_size > a1_threshold && a1_head != INVALID_FRAME)
	{
		victim = a1_pop_front();
	}
	else if (a2_head != INVALID_FRAME)
	{
		victim = a2_pop_front();
	}
	else if (a1_head != INVALID_FRAME)
	{
		victim = a1_pop_front();
	}
	else
	{
		for (pfn_t f = 0; f < (pfn_t)memsize; f++)
		{
			frame_t *fr = frame_from_number(f);
			if (fr != NULL && frame_in_use(fr))
			{
				victim = f;
				break;
			}
		}
		if (victim == INVALID_FRAME)
		{
			victim = 0;
		}
	}

	s2q_states[victim] = S2Q_STATE_NONE;
	s2q_next[victim] = INVALID_FRAME;
	s2q_prev[victim] = INVALID_FRAME;

	return victim;
}

/**
 * @brief Called on each access to a page to update any information
 * needed by the simplified 2Q algorithm.
 *
 * @param framenum[in] The frame number being accessed.
 */
void s2q_ref(pfn_t framenum)
{
	frame_t *frame = frame_from_number(framenum);
	set_referenced(frame, true);

	switch (s2q_states[framenum])
	{
	case S2Q_STATE_NONE:
		a1_insert_back(framenum);
		s2q_states[framenum] = S2Q_STATE_A1;
		break;

	case S2Q_STATE_A1:
		a1_remove(framenum);
		a2_insert_back(framenum);
		s2q_states[framenum] = S2Q_STATE_A2;
		break;

	case S2Q_STATE_A2:
		a2_remove(framenum);
		a2_insert_back(framenum);
		break;
	}
}

/**
 * @brief Initialize data structures for the simplified 2Q algorithm.
 */
void s2q_init(void)
{
	s2q_states = malloc369(memsize * sizeof(s2q_state_t));
	s2q_next = malloc369(memsize * sizeof(pfn_t));
	s2q_prev = malloc369(memsize * sizeof(pfn_t));

	memset(s2q_states, 0, memsize * sizeof(s2q_state_t));
	for (size_t i = 0; i < memsize; i++)
	{
		s2q_next[i] = INVALID_FRAME;
		s2q_prev[i] = INVALID_FRAME;
	}

	a1_head = a1_tail = INVALID_FRAME;
	a2_head = a2_tail = INVALID_FRAME;
	a1_size = 0;
	a2_size = 0;

	a1_threshold = memsize / 10;
	if (a1_threshold == 0)
	{
		a1_threshold = 1;
	}
}

/**
 * @brief Clean up data structures used by the simplified 2Q algorithm.
 */
void s2q_cleanup(void)
{
	if (s2q_states != NULL)
	{
		free369(s2q_states);
		s2q_states = NULL;
	}
	if (s2q_next != NULL)
	{
		free369(s2q_next);
		s2q_next = NULL;
	}
	if (s2q_prev != NULL)
	{
		free369(s2q_prev);
		s2q_prev = NULL;
	}

	a1_head = a1_tail = INVALID_FRAME;
	a2_head = a2_tail = INVALID_FRAME;
	a1_size = 0;
	a2_size = 0;
	a1_threshold = 0;
}
