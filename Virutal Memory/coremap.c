#include "multiprocessing.h"
#include "coremap.h"
#include "ptrarray.h"
#include "types.h"
#include "malloc369.h"
#include "list.h"

#include <string.h>
#include <assert.h>

struct frame {
	/* Pointer to array of pointers back to pagetable entry (pte) for
	 * pages that reference this frame. */
	ptrarray_t *refs;

	/* For evict algorithm */
	list_entry framelist_entry;

	/* The ASID the frame belongs to, or INVALID_ASID */
	asid_t asid;

	/* Recently referenced marker */
	bool refd;
};

static size_t mem_usage = 0;

static inline ptrarray_t *
get_refs(const frame_t *frame)
{
	return frame->refs;
}

static inline void
set_refs(frame_t *frame, ptrarray_t *arr)
{
	frame->refs = arr;
}

bool
frame_in_use(const frame_t *frame)
{
	const ptrarray_t *const refs = get_refs(frame);
	if (refs == NULL)
		return false;

	return ptrarray_get_size(refs) > 0;
}

bool
frame_is_shared(const frame_t *frame)
{
	return ptrarray_get_size(get_refs(frame)) > 1;
}

frame_t *
frame_from_number(pfn_t framenum)
{
	if (framenum == INVALID_FRAME || (size_t)framenum > memsize)
		return NULL;
	else
		return &coremap[framenum];
}

frame_t *
frame_from_list_entry(list_entry *entry)
{
	return container_of(entry, frame_t, framelist_entry);
}

ptrarray_slice_t __nonnull()
get_referring_ptes(const frame_t *f)
{
	return ptrarray_get_slice(get_refs(f), 0, UINT16_MAX);
}

pfn_t __nonnull()
get_frame_number(const frame_t *f)
{
	return f - coremap;
}

list_entry * __nonnull()
get_frame_list_entry(frame_t *pframe)
{
	return &pframe->framelist_entry;
}

bool
get_referenced(const frame_t *frame)
{
	return frame->refd;
}

void
set_referenced(frame_t *frame, bool val)
{
	frame->refd = val;
}

pfn_t
allocate_frame(pt_entry_t *pte)
{
	static i32 last_alloc = -1;
	pfn_t frame = INVALID_FRAME;

	// Allocate an available frame from where we left off last time
	if (mem_usage < memsize) {
		i32 i = (last_alloc + 1) % memsize;
		while (i != last_alloc) {
			if (!frame_in_use(&coremap[i])) {
				frame = i;
				last_alloc = i;
				mem_usage += 1;
				break;
			}
			i = (i + 1) % memsize;
		}
	}
	frame_t *f = frame_from_number(frame);

	if (frame == INVALID_FRAME) { // Didn't find a free page.
		// Call replacement algorithm's evict function to select victim
		frame = evict_func();
		f = frame_from_number(frame);

		// All frames were in use, so victim frame must hold some page
		// Write victim page to swap, if needed, and update page table
		assert(f != NULL);
		assert(frame_in_use(f));

		handle_frame_evict(frame, f->asid);
		ptrarray_clear(get_refs(f));
	}

	assert(f != NULL);

	// Record information for virtual page that will now be stored in frame
	if (get_refs(f) == NULL)
		set_refs(f, ptrarray_init(1, PTRARRAY_DEFAULT_PRESSURE));
	ptrarray_append(get_refs(f), pte);
	f->asid = current_task_id();

	assert(frame != INVALID_FRAME);
	return frame;
}

void
frame_link_pte(pfn_t framenum, pt_entry_t *pte)
{
	frame_t *const f = frame_from_number(framenum);
	assert(f != NULL);
	ptrarray_append(get_refs(f), pte);
}

void
frame_unlink_pte(pfn_t framenum, pt_entry_t *pte)
{
	frame_t *const f = frame_from_number(framenum);
	assert(f != NULL);
	ptrarray_remove(get_refs(f), pte);
	if (ptrarray_get_size(get_refs(f)) == 0) {
		mem_usage--;
	}
}

void
init_coremap(void)
{
	coremap = malloc369(memsize * sizeof(struct frame));
	assert(coremap != NULL);
	memset(coremap, 0, memsize * sizeof(struct frame));
	for (size_t i = 0; i < memsize; i += 1)
		coremap[i].asid = INVALID_ASID;
}

void
destroy_coremap(void)
{
	for (size_t i = 0; i < memsize; i += 1) {
		if (get_refs(&coremap[i]) != NULL)
			ptrarray_destroy(get_refs(&coremap[i]));
	}

	free369(coremap);
}

/*
 * Initializes the content of a (simulated) physical memory frame when it
 * is first allocated for some virtual address. Just like in a real OS, we
 * fill the frame with zeros to prevent leaking information across pages.
 */
void
init_frame(pfn_t frame)
{
	// Calculate pointer to start of frame in (simulated) physical memory
	u8 *mem_ptr = &physmem[frame * SIMPAGESIZE];
	memset(mem_ptr, 0, SIMPAGESIZE); // zero-fill the frame
}
