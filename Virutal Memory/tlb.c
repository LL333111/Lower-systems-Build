/** @file tlb.c
 * @brief Software TLB Implementation
 * 
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * @author Nagata Parama Aptana
 * @author Louis Ryan Tan
 * @author Kuei (Jack) Sun
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdalign.h>

#include "tlb.h"
#include "sim.h"
#include "types.h"

typedef enum tlb_result_e {
    TLB_WRITE_FAULT = -2,
    TLB_FAULT = -1,
    TLB_SUCCESS = 0
} tlb_result_t;

static alignas(4096) struct {
	u64 keys[255];
	u8 _padding;
	u64 values[255];
	u8 size;
} tlb;

#define VALID_MASK (1ULL << 40)

static size_t __tlb_hit_count = 0;
static size_t __tlb_miss_count = 0;

void
init_soft_tlb(struct tlb_config * cfg)
{
	assert(cfg->size <= TLB_MAXIMUM_SIZE);
	srand(cfg->seed);
	memset(&tlb, 0, sizeof(tlb));
	tlb.size = cfg->size > 0 ? cfg->size : TLB_DEFAULT_SIZE;
}

void
destroy_soft_tlb(void)
{
	memset(&tlb, 0, sizeof(tlb));
}

// for the following functions, registers are reassigned to follow
// MIPS specification, difference is the reassignment is 
// 'const' if it's not modified, and not 'const' otherwise

i32
tlbwi(tlb_index_t idx, const tlb_entry_t *entry)
{
	if (__builtin_expect(idx >= tlb.size, false))
		return TLB_FAULT;

	tlb.keys[idx] = entry->half.high;
	tlb.values[idx] = entry->half.low;
	return TLB_SUCCESS;
}

i32
tlbr(tlb_index_t idx, tlb_entry_t *entry)
{
	if (__builtin_expect(idx >= tlb.size, false))
		return TLB_FAULT;

	entry->half.high = tlb.keys[idx];
	entry->half.low = tlb.values[idx];
	return TLB_SUCCESS;
}


#if defined(__x86_64__) && defined(__AVX2__)
// let this function be a black box
#pragma GCC optimize("Ofast")
[[maybe_unused]] [[gnu::hot]]
tlb_index_t
tlbp(asid_t asid, vpn_t vpn)
{

	static const u64 mask = ((u64)ASID_MASK << 48) | VALID_MASK | VPN_MASK;
	static const __m256i mask_vec = { mask, mask, mask, mask };

	const u64 target = ((u64)asid << 48) | VALID_MASK | vpn;
	__m256i target_vec = _mm256_set1_epi64x(target);

	#pragma GCC unroll 4
	for (u16 i = 0; i < tlb.size; i += 4) {
		__m256i current_vec =
			_mm256_load_si256((const __m256i *)&tlb.keys[i]);

		__m256i current_masked =
			_mm256_and_si256(current_vec, mask_vec);

		__m256i comparison =
			_mm256_cmpeq_epi64(current_masked, target_vec);

		i32 movemask = _mm256_movemask_epi8(comparison);

		if (movemask != 0)
			return i + (__builtin_ctz(movemask) >> 3);
	}

	return TLB_PROBE_NOTFOUND;
}
#else

#pragma message "\n" \
"---------------------------------------------------------------------------------------\n" \
" Your CPU does not support AVX2, compiling tlbp with generic fallback.\n" \
" On teach.cs testing server, tlbp is optimized using AVX2.\n" \
"\n" \
" Learn more:\n" \
" https://en.wikipedia.org/wiki/Advanced_Vector_Extensions#Advanced_Vector_Extensions_2\n" \
"---------------------------------------------------------------------------------------\n"

// let this function be a black box
#pragma GCC optimize("Ofast")
[[maybe_unused]] [[gnu::hot]]
tlb_index_t
tlbp(asid_t asid, vpn_t vpn)
{
	const u64 target = ((u64)asid << 48) | VALID_MASK | vpn;

	for (u16 i = 0; i < tlb.size; i += 1) {
		if (tlb.keys[i] == target)
			return i;
	}
	return TLB_PROBE_NOTFOUND;
}
#endif

i32 
tlbwr(const tlb_entry_t * entry)
{
	tlb_index_t vacant = random() % tlb.size;

	tlb.keys[vacant] = entry->half.high;
	tlb.values[vacant] = entry->half.low;
	return 0;
}

static tlb_result_t
tlb_resolve_addr(char type, asid_t asid, vaddr_t vaddr, paddr_t * res)
{
	u64 offset = vaddr % PAGE_SIZE;
	vpn_t vpn = vaddr >> PAGE_SHIFT;

	const tlb_index_t idx = tlbp(asid, vpn);

	if (idx == TLB_PROBE_NOTFOUND)
		return TLB_FAULT;

	tlb_entry_t to_read;
	tlbr(idx, &to_read);
	assert(to_read.fields.valid);
	assert(to_read.fields.asid == asid);
	assert(to_read.fields.vpn == vpn);

	if ((type == 'S' || type == 'M')
	    && !to_read.fields.dirty) {
		/* attempting to write to a clean page causes a write fault */
		return TLB_WRITE_FAULT;
	}

	*res = ((paddr_t)to_read.fields.pfn << PAGE_SHIFT) + offset;
	return TLB_SUCCESS;
}

paddr_t 
tlb_translate(char type, asid_t asid, pagetable_t *const pt, vaddr_t vaddr)
{
	paddr_t memaddr;

	enum {
		NO_FAULT,
		WRITE_FAULT,
		MISS_FAULT,
	} fault_type = NO_FAULT;
	tlb_result_t err = tlb_resolve_addr(type, asid, vaddr, &memaddr);

retry:
	switch (err) {
		case TLB_SUCCESS:
			__tlb_hit_count += (fault_type == NO_FAULT) ? 1 : 0;
			break;
		case TLB_FAULT:
			// can only get here if haven't previously faulted
			assert(fault_type == NO_FAULT);

			handle_tlb_fault(asid, pt, vaddr, type, false);
			err = tlb_resolve_addr(type, asid, vaddr, &memaddr); // retry
			__tlb_miss_count += (fault_type == NO_FAULT) ? 1 : 0;  // don't double count

			// can only write fault or succeed
			assert(err == TLB_WRITE_FAULT || err == TLB_SUCCESS);
			fault_type = MISS_FAULT;
			goto retry;
		case TLB_WRITE_FAULT:
			// cannot get here if previous fault is a write fault
			assert(fault_type == NO_FAULT || fault_type == MISS_FAULT);

			handle_tlb_fault(asid, pt, vaddr, type, true);
			err = tlb_resolve_addr(type, asid, vaddr, &memaddr); // retry
			__tlb_miss_count += (fault_type == NO_FAULT) ? 1 : 0;  // don't double count
			
			assert(err == TLB_SUCCESS); // if fail again something is wrong
			fault_type = WRITE_FAULT;
			goto retry;
	}

	return memaddr;
}

size_t
tlb_hit_count(void)
{
	return __tlb_hit_count;
}

size_t
tlb_miss_count(void)
{
	return __tlb_miss_count;
}
