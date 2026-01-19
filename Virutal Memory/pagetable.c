/**
 * @file pagetable.c
 * @brief Students' page table implementation
 *
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * @author Andrew Peterson
 * @author Karen Reid
 * @author Alexey Khrabrov
 * @author Angela Brown
 * @author Kuei (Jack) Sun
 * @author Nagata Parama Aptana
 * @author Louis Ryan Tan
 *
 * All of the files in this directory and all subdirectories are:
 * @copyright Copyright (c) 2019, 2021 Karen Reid
 * @copyright Copyright (c) 2023, Angela Brown, Kuei (Jack) Sun
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "malloc369.h"
#include "ptrarray.h"
#include "sim.h"
#include "coremap.h"
#include "swap.h"
#include "tlb.h"
#include "types.h"
#include "pagetable.h"

extern int current_task_id();
extern struct task_s *current_task();
struct pt_entry_s
{
	int swapped;
	int writable;
	int valid;
	int dirty;
	pfn_t pfn;
	off_t swap_offset;
	vpn_t vpn;
};
struct pagetable_l4
{
	pt_entry_t pages[512];
};

struct pagetable_l3
{
	struct pagetable_l4 *l3[512];
};
struct pagetable_l2
{
	struct pagetable_l3 *l2[512];
};
struct pagetable
{
	struct pagetable_l2 *l1[512];
};

// Counters for various events.
// Your code must increment these when the related events occur.
size_t ram_hit_count = 0;
size_t ram_miss_count = 0;
size_t ref_count = 0;
size_t evict_clean_count = 0;
size_t evict_dirty_count = 0;
size_t cow_fault_count = 0;
size_t write_fault_count = 0;

/* Allocate zeroed pages.
 */
__attribute__((unused)) static void *
alloc_zeroed_pages(size_t npages)
{
	void *pages = malloc369(npages * PAGE_SIZE);
	if (pages != NULL)
	{
		memset(pages, 0, npages * PAGE_SIZE);
	}
	return pages;
}

bool __nonnull() is_valid_pte(const pt_entry_t *pte)
{
	return pte->valid;
}

bool __nonnull() is_dirty_pte(const pt_entry_t *pte)
{
	return pte->dirty;
}

bool __nonnull() is_swapped_pte(const pt_entry_t *pte)
{
	return pte->swapped;
}

bool __nonnull() is_readonly_pte(const pt_entry_t *pte)
{
	return !pte->writable;
}

/* Returns true if a write of type `type` to page referenced by pte results in
 * a CoW fault, otherwise false.
 */
__attribute__((unused)) static __nonnull() bool is_cow_fault(pt_entry_t *pte, char type)
{
	return (type == 'S' || type == 'M') &&
		   !pte->writable &&
		   pte->valid &&
		   !pte->swapped;
}

pfn_t __nonnull() framenum_from_pte(const pt_entry_t *pte)
{
	return pte->pfn;
}

pagetable_t *create_pagetable(void)
{
	pagetable_t *pt = malloc369(sizeof(pagetable_t));
	if (pt != NULL)
	{
		memset(pt->l1, 0, sizeof(pt->l1));
	}
	return pt;
}

/* Update pte information after its referenced frame just got evicted.
 *
 * Update the tlb as well to ensure consistent state.
 */
__attribute__((unused)) static void
handle_pte_evict(pt_entry_t *pte, off_t swap_offset, asid_t asid)
{
	pte->valid = 0;
	if (swap_offset != INVALID_SWAP)
	{
		pte->swapped = 1;
		pte->swap_offset = swap_offset;
	}
	else
	{
		pte->swapped = 0;
		pte->swap_offset = swap_offset;
	}
	pte->pfn = INVALID_FRAME;
	tlb_index_t idx = tlbp(asid, pte->vpn);
	if (idx != TLB_PROBE_NOTFOUND)
	{
		tlb_entry_t entry;
		tlbr(idx, &entry);
		entry.fields.valid = false;
		tlbwi(idx, &entry);
	}
}

__attribute__((unused)) void
handle_frame_evict(pfn_t framenum, asid_t asid)
{
	frame_t *frame = frame_from_number(framenum);
	ptrarray_slice_t ptes = get_referring_ptes(frame);

	for (int i = 0; i < ptes.len; i++)
	{
		pt_entry_t *pte = (pt_entry_t *)ptes.ptr[i];
		off_t swap_offset = INVALID_SWAP;
		if (pte->dirty)
		{
			evict_dirty_count++;
			swap_offset = swap_pageout(framenum, pte->swap_offset);
			pte->dirty = 0;
		}
		else
		{
			evict_clean_count++;
			swap_offset = pte->swap_offset;
		}
		handle_pte_evict(pte, swap_offset, asid);
		frame_unlink_pte(framenum, pte);
	}
}

/* Copy the frame with number `src` to frame number `dst` and
 * return the pointer to the frame with number `dst`.
 */
__attribute__((unused)) static void *
copy_frame(pfn_t dst, pfn_t src)
{
	extern u8 *physmem;
	void *src_ptr = &physmem[src * PAGE_SIZE];
	void *dst_ptr = &physmem[dst * PAGE_SIZE];
	memcpy(dst_ptr, src_ptr, PAGE_SIZE);
	return dst_ptr;
}

/*
 * Locate the physical frame number for the given vaddr using the page table.
 *
 * If the page table entry is invalid and not on swap, then this is the first
 * reference to the page and a (simulated) physical frame should be allocated
 * and initialized to all zeros (using init_frame from coremap.c).
 * If the page table entry is invalid and on swap, then a (simulated) physical
 * frame should be allocated and filled by reading the page data from swap.
 *
 * Make sure to update page table entry status information:
 *  - the page table entry should be marked valid
 *  - if the type of access is a write ('S'tore or 'M'odify),
 *    the page table entry should be marked dirty
 *  - a page should be marked dirty on the first reference to the page,
 *    even if the type of access is a read ('L'oad or 'I'nstruction type).
 *  - DO NOT UPDATE the page table entry 'referenced' information. That
 *    should be done by the replacement algorithm functions.
 *
 * When you have a valid page table entry, return the page frame number
 * that holds the requested virtual page.
 *
 * Counters for hit, miss and reference events should be incremented in
 * this function.
 */

__attribute__((unused)) static pfn_t
find_frame_number(pt_entry_t *pte, char type)
{
	ref_count++;
	if (pte->valid)
	{
		ram_hit_count++;
		if ((type == 'S' || type == 'M'))
		{
			pte->dirty = 1;
		}
		return pte->pfn;
	}

	ram_miss_count++;
	pfn_t frame = allocate_frame(pte);

	if (pte->swapped)
	{
		swap_pagein(frame, pte->swap_offset);
		pte->swapped = 0;

		if ((type == 'S' || type == 'M'))
		{
			pte->dirty = 1;
		}
	}
	else
	{
		init_frame(frame);
		pte->dirty = 1;
	}

	pte->pfn = frame;
	pte->valid = 1;

	return frame;
}

pt_entry_t *page_walk(pagetable_t *pt, vaddr_t vaddr, char type)
{
	vpn_t vpn = vaddr >> 12;
	size_t i1 = (vpn >> 27) & 0x1FF;
	size_t i2 = (vpn >> 18) & 0x1FF;
	size_t i3 = (vpn >> 9) & 0x1FF;
	size_t page_index = vpn & 0x1FF;

	if (pt->l1[i1] == NULL)
	{
		pt->l1[i1] = malloc369(sizeof(struct pagetable_l2));
		memset(pt->l1[i1]->l2, 0, sizeof(pt->l1[i1]->l2));
	}

	struct pagetable_l2 *l2 = pt->l1[i1];
	if (l2->l2[i2] == NULL)
	{
		l2->l2[i2] = malloc369(sizeof(struct pagetable_l3));
		memset(l2->l2[i2]->l3, 0, sizeof(l2->l2[i2]->l3));
	}

	struct pagetable_l3 *l3 = l2->l2[i2];
	if (l3->l3[i3] == NULL)
	{
		l3->l3[i3] = malloc369(sizeof(struct pagetable_l4));
		memset(l3->l3[i3]->pages, 0, sizeof(l3->l3[i3]->pages));
	}
	struct pagetable_l4 *l4 = l3->l3[i3];
	pt_entry_t *pte = &l4->pages[page_index];

	if (!pte->valid && !pte->swapped && pte->pfn == 0 && pte->vpn == 0)
	{
		pte->vpn = vpn;
		pte->writable = 1;
		pte->dirty = 0;
		pte->swapped = 0;
		pte->swap_offset = INVALID_SWAP;
		pte->pfn = INVALID_FRAME;
	}

	(void)find_frame_number(pte, type);
	return pte;
}

void free_pagetable(pagetable_t *pt)
{
	if (pt == NULL)
	{
		return;
	}
	for (size_t i = 0; i < 512; i++)
	{
		struct pagetable_l2 *l2 = pt->l1[i];
		if (l2 == NULL)
		{
			continue;
		}
		for (size_t j = 0; j < 512; j++)
		{
			struct pagetable_l3 *l3 = l2->l2[j];
			if (l3 == NULL)
			{
				continue;
			}
			for (size_t k = 0; k < 512; k++)
			{
				struct pagetable_l4 *l4 = l3->l3[k];
				if (l4 == NULL)
				{
					continue;
				}
				for (size_t m = 0; m < 512; m++)
				{
					pt_entry_t *pte = &l4->pages[m];
					if (pte->valid)
					{
						frame_unlink_pte(pte->pfn, pte);
					}
					if (pte->swapped && pte->swap_offset != INVALID_SWAP)
					{
						swap_free(pte->swap_offset);
					}
				}
				free369(l4);
			}
			free369(l3);
		}
		free369(l2);
	}
	free369(pt);
}

pagetable_t *
duplicate_pagetable(pagetable_t *src, asid_t src_asid)
{
	pagetable_t *child = create_pagetable();
	if (child == NULL)
	{
		return NULL;
	}
	for (size_t i = 0; i < 512; i++)
	{
		struct pagetable_l2 *l2 = src->l1[i];
		if (l2 == NULL)
		{
			continue;
		}
		for (size_t j = 0; j < 512; j++)
		{
			struct pagetable_l3 *l3 = l2->l2[j];
			if (l3 == NULL)
			{
				continue;
			}
			for (size_t k = 0; k < 512; k++)
			{
				struct pagetable_l4 *l4 = l3->l3[k];
				if (l4 == NULL)
				{
					continue;
				}
				for (size_t m = 0; m < 512; m++)
				{
					pt_entry_t *src_pte = &l4->pages[m];
					if (!src_pte->valid)
					{
						continue;
					}
					// Allocate l2 and l3 if not exist
					if (child->l1[i] == NULL)
					{
						child->l1[i] = malloc369(sizeof(struct pagetable_l2));
						memset(child->l1[i]->l2, 0, sizeof(child->l1[i]->l2));
					}
					// Allocate l3 if not exist
					if (child->l1[i]->l2[j] == NULL)
					{
						child->l1[i]->l2[j] = malloc369(sizeof(struct pagetable_l3));
						memset(child->l1[i]->l2[j]->l3, 0, sizeof(child->l1[i]->l2[j]->l3));
					}
					// Allocate l4 if not exist
					if (child->l1[i]->l2[j]->l3[k] == NULL)
					{
						child->l1[i]->l2[j]->l3[k] = malloc369(sizeof(struct pagetable_l4));
						memset(child->l1[i]->l2[j]->l3[k]->pages, 0, sizeof(child->l1[i]->l2[j]->l3[k]->pages));
					}
					// Get child pte
					pt_entry_t *child_pte = &child->l1[i]->l2[j]->l3[k]->pages[m];
					// Copy src pte to child pte
					*child_pte = *src_pte;
					// Set both src and child pte to read-only
					src_pte->writable = 0;
					child_pte->writable = 0;
					// Link child pte to frame
					frame_link_pte(child_pte->pfn, child_pte);
				}
			}
		}
	}
	// set TLB dirty to unwritable
	for (tlb_index_t idx = 0; idx < TLB_MAXIMUM_SIZE; idx++)
	{
		tlb_entry_t entry;
		if (tlbr(idx, &entry) != 0)
		{
			continue;
		}

		if (!entry.fields.valid)
		{
			continue;
		}

		if (entry.fields.asid != src_asid)
		{
			continue;
		}
		entry.fields.valid = false;
		tlbwi(idx, &entry);
	}
	return child;
}

void handle_tlb_fault(asid_t asid, pagetable_t *pt, vaddr_t vaddr, char type, bool write)
{
	bool is_write_access = (type == 'S' || type == 'M');
	pt_entry_t *pte = page_walk(pt, vaddr, type);

	if (write && is_write_access)
	{
		write_fault_count++;
		if (pte->valid && !pte->swapped && !pte->writable)
		{
			// Treat any write to a valid read-only page as a CoW fault.
			cow_fault_count++;

			pfn_t old_frame = pte->pfn;
			frame_t *old_fr = frame_from_number(old_frame);
			assert(old_fr != NULL);

			pfn_t new_frame = allocate_frame(pte);
			copy_frame(new_frame, old_frame);

			// Fix frame <-> pte links
			frame_unlink_pte(old_frame, pte);
			frame_link_pte(new_frame, pte);

			pte->pfn = new_frame;
			pte->swapped = 0;
			pte->swap_offset = INVALID_SWAP;
			pte->writable = 1;
		}

		// After a write fault, we are writing to this page.
		pte->dirty = 1;
	}

	tlb_entry_t entry;
	memset(&entry, 0, sizeof(entry));

	vpn_t vpn = vaddr >> PAGE_SHIFT;
	entry.fields.vpn = vpn;
	entry.fields.pfn = pte->pfn;
	entry.fields.asid = asid;
	entry.fields.valid = 1;

	if (write && is_write_access && pte->writable)
	{
		entry.fields.dirty = 1;
	}
	else
	{
		entry.fields.dirty = 0;
	}

	tlb_index_t idx = tlbp(asid, vpn);
	if (idx != TLB_PROBE_NOTFOUND)
	{
		tlbwi(idx, &entry);
	}
	else
	{
		tlbwr(&entry);
	}
}
