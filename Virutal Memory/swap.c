/** @file swap.c
 * @brief Swap space implementation for the simulator.
 *
 * @note The current swap implementation is memory-backed rather than 
 *       file-backed, for faster testing time.
 */

#include <assert.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "malloc369.h"
#include "sim.h"
#include "types.h"
#include "swap.h"

/*
 * Swap-related stats counters
 */

static size_t swapin_count = 0;
static size_t swapout_count = 0;

/*
 * Bitmap definitions and functions.
 */

static const size_t bits_per_word = sizeof(size_t) * CHAR_BIT;
static const size_t word_all_bits = (size_t)-1;

static size_t nwords_for_nbits(size_t nbits)
{
	return (nbits + bits_per_word - 1) / bits_per_word;
}

struct bitmap {
	size_t nbits;
	size_t *words;
};

static i32
bitmap_init(struct bitmap *b, size_t nbits)
{
	size_t nwords = nwords_for_nbits(nbits);
	b->words = malloc369(nwords * sizeof(size_t));
	if (!b->words) {
		return -1;
	}

	memset(b->words, 0, nwords * sizeof(size_t));
	b->nbits = nbits;

	// Mark any leftover bits at the end in use
	if (nwords > nbits / bits_per_word) {
		size_t idx = nwords - 1;
		size_t overbits = nbits - idx * bits_per_word;

		assert(nbits / bits_per_word == nwords - 1);
		assert(overbits > 0 && overbits < bits_per_word);

		for (size_t j = overbits; j < bits_per_word; ++j) {
			b->words[idx] |= ((size_t)1 << j);
		}
	}

	return 0;
}

static i32
bitmap_alloc(struct bitmap *b, size_t *index)
{
	size_t max_idx = nwords_for_nbits(b->nbits);

	for (size_t idx = 0; idx < max_idx; ++idx) {
		if (b->words[idx] != word_all_bits) {
			for (size_t offset = 0; offset < bits_per_word; ++offset) {
				size_t mask = (size_t)1 << offset;

				if ((b->words[idx] & mask) == 0) {
					b->words[idx] |= mask;
					*index = (idx * bits_per_word) + offset;
					assert(*index < b->nbits);
					return 0;
				}
			}
			assert(false);
		}
	}
	return -1;
}

static void
bitmap_free(struct bitmap *b, size_t index)
{
	assert(index < b->nbits);
	const ldiv_t pos = ldiv(index, bits_per_word);
	const size_t mask = 1UL << pos.rem;
	b->words[pos.quot] &= ~mask;
}

static void
bitmap_destroy(struct bitmap *b)
{
	free369(b->words);
}

/*
 * Swap definitions and functions.
 */

static struct bitmap swapmap;
static alignas(16) u8 *swap_addr;

void
swap_init(size_t size)
{
	// Initialize the swap space
	assert(swap_addr == NULL);
	swap_addr = malloc369(size * SIMPAGESIZE);
	if (swap_addr == NULL) {
		perror("Failed to allocate memory for virtual swap");
		exit(1);
	}

	// Initialize the bitmap
	if (bitmap_init(&swapmap, size) != 0) {
		free369(swap_addr);
		swap_addr = NULL;
		perror("Failed to create bitmap for swap\n");
		exit(1);
	}
}

void
swap_destroy(void)
{
	free369(swap_addr);
	swap_addr = NULL;
	bitmap_destroy(&swapmap);
}

i32
swap_pagein(pfn_t frame, off_t offset)
{
	swapin_count++;
	assert(offset != INVALID_SWAP);

	// Get pointer to page data in (simulated) physical memory
	void *frame_ptr = &physmem[frame * SIMPAGESIZE];
	const void *swap_ptr = &swap_addr[offset];

	memcpy(frame_ptr, swap_ptr, SIMPAGESIZE);
	return 0;
}

off_t
swap_pageout(pfn_t frame, off_t offset)
{
	swapout_count++;
	// Check if swap has already been allocated for this page
	if (offset == INVALID_SWAP) {
		size_t idx;
		if (bitmap_alloc(&swapmap, &idx) != 0) {
			fprintf(stderr, "swap_pageout: Could not allocate swap space. "
			                "Try running again with a larger swapsize.\n");
			return INVALID_SWAP;
		}
		offset = idx * SIMPAGESIZE;
	}
	assert(offset != INVALID_SWAP);

	// Get pointer to page data in (simulated) physical memory
	const void *frame_ptr = &physmem[frame * SIMPAGESIZE];
	void *swap_ptr = &swap_addr[offset];

	memcpy(swap_ptr, frame_ptr, SIMPAGESIZE);
	return offset;
}

void
swap_free(off_t offset)
{
	bitmap_free(&swapmap, offset / SIMPAGESIZE);
}

size_t
swap_pagein_count(void)
{
	return swapin_count;
}

size_t
swap_pageout_count(void)
{
	return swapout_count;
}
