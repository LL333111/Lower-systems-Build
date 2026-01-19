/** @file tlb.h
 * @brief Software TLB Header File
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
 *
 */
#ifndef __TLB_H__
#define __TLB_H__

#include "pagetable.h"
#include "types.h"

#if defined(__x86_64__) && defined(__AVX2__)
#include <immintrin.h>
#endif


/* A: ASID
 * V: Valid flag
 * P: Virtual page number
 * D: Dirty flag
 * F: Physical frame number
 *
 * 127 | AAAAAAAA AAAAAAAA -------V ----PPPP | 96
 *  95 | PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP | 64
 *  63 | -------D -------- FFFFFFFF FFFFFFFF | 32
 *  31 | FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF |  0
 */
typedef union {
	struct {
		pfn_t pfn	: 48;
		u8 _padding;
		bool dirty;
		vpn_t vpn	: 36;
		bool valid;
		asid_t asid;
	} fields;

	struct { u64 low; u64 high; } half;
} tlb_entry_t;

typedef u8 tlb_index_t;

/**
 * @brief Translate a virtual address to physical address using the translation
 * lookaside buffer, with page table fallback on a miss.

 * @param type[in]  The memory access type.
 * @param asid[in]  The address-space identifier.
 * @param pt[in]    The page table to consult on a TLB miss (non-null).
 * @param vaddr[in] The virtual address to translate.
 *
 * @return The corresponding physical address after a successful translation. 
 * 
 * @see tlb.c
 */
paddr_t tlb_translate(char type, asid_t asid, pagetable_t *const pt, vaddr_t vaddr);

struct tlb_config;

// TLB functions used in sim.c for initialization and teardown
void init_soft_tlb(struct tlb_config * cfg);
void destroy_soft_tlb(void);

#define TLB_DEFAULT_SIZE  64
#define TLB_MAXIMUM_SIZE 255
#define TLB_PROBE_NOTFOUND ((tlb_index_t) -1)

/*
 * hardware primitives
 * nonzero return means tlb fault, to be handled by software
 *
 **/

/**
 * @brief TLB write indexed.
 * 
 * This function writes a TLB entry to the specified index.
 * 
 * @param idx[in] The index to write the TLB entry to.
 * @param entry[in] The TLB entry to write.
 * @return TLB_SUCCESS on success, TLB_FAULT on failure.
 * 
 * @see tlb.c
 */
i32 tlbwi(tlb_index_t idx, const tlb_entry_t * entry);

/**
 * @brief TLB read.
 * 
 * Reads a TLB entry from the specified index.
 * 
 * @param idx[in] The index to read the TLB entry from.
 * @param entry[out] The TLB entry read from the specified index.
 * @return TLB_SUCCESS on success, TLB_FAULT on failure.
 * 
 * @see tlb.c
 */
i32 tlbr(tlb_index_t idx, tlb_entry_t * entry);

/**
 * @brief TLB probe.
 * 
 * This function probes the TLB for a matching entry.
 * 
 * @param asid[in] The address space identifier to search for.
 * @param vpn[in] The virtual page number to search for.
 * @return The index of the matching TLB entry, or
 * TLB_PROBE_NOTFOUND if not found.
 * 
 * @see tlb.c
 */
tlb_index_t tlbp(asid_t asid, vpn_t vpn);

/**
 * @brief TLB write random.
 * 
 * This function writes a TLB entry to a random index.
 * 
 * @param entry[in] The TLB entry to write.
 * @return TLB_SUCCESS on success, TLB_FAULT on failure.
 * 
 * @see tlb.c
 */
i32 tlbwr(const tlb_entry_t * entry);

/**
 * @brief Return the number of tlb hits thus far in the simulation.
 *
 * @return The number of tlb hits.
 *
 * @see tlb.c
 */
extern size_t tlb_hit_count(void);

/**
 * @brief Return the number of tlb misses thus far in the simulation.
 *
 * @return The number of tlb misses.
 *
 * @see tlb.c
 */
extern size_t tlb_miss_count(void);

#endif
