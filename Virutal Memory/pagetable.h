#ifndef __PAGETABLE_H__
#define __PAGETABLE_H__

#include "types.h"

/* User-level virtual addresses on a 64-bit Linux system are 48 bits in our
 * traces, and the page size is 4096 (12 bits). The remaining 36 bits are
 * the virtual page number, which is used as the lookup key (or index) into
 * your page table.
 */

/**
 * @brief Page table entry struct.
 *
 * This structure will need to record the physical page frame number for a
 * virtual page, as well as the swap offset if it is evicted. You will also
 * need to keep track of the Valid, Dirty and Referenced status or flags.
 * You do not need to keep track of Read/Execute permissions.
 *
 * The format of the page table, and thus what you need to do to get ready
 * to start translating virtual addresses, is up to you.
 *
 * @note STUDENT IMPLEMENTATION
 */
typedef struct pt_entry_s pt_entry_t;

/**
 * @brief Page table handle struct.
 *
 * This structure refers to a page table of a specific process. Its lifetime is
 * managed by `create_pagetable`, `duplicate_pagetable`, and `free_pagetable`.
 *
 * @note STUDENT IMPLEMENTATION
 */
typedef struct pagetable pagetable_t;

/**
 * @brief Initializes a page table.
 *
 * This function is called at each creation of a process in the simulation.
 *
 * @return The newly created page table.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
pagetable_t *create_pagetable(void);

/**
 * @brief Destroys a page table and frees its memory.
 *
 * This function is called when a process dies.
 *
 * @param[in] pt The would-be destroyed page table.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
void free_pagetable(pagetable_t *pt);

/**
 * @brief Duplicates an existing page table.
 *
 * This function is called when a process forks.
 *
 * @param[in] src The pagetable of the parent process.
 * @param[in] src_asid The address space identifier of the parent process.
 * @return The page table of the child process.
 * @pre The parent process must be fully initialized and in a valid state.
 * @post All parent and child page table entries must be in a read-only state.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
pagetable_t *duplicate_pagetable(pagetable_t *src, asid_t src_asid);

/**
 * @brief Find the appropriate page table entry for a memory access.
 *
 * This function could trigger memory allocation, frame eviction, swap device
 * read, or create new page table entries if necessary.
 * 
 * @param[in] pt The page table being searched.
 * @param[in] vaddr The virtual address being accessed.
 * @param[in] type The memory access type.
 * @return The page table entry that corresponds the virtual address.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
pt_entry_t *page_walk(pagetable_t *pt, vaddr_t vaddr, char type);

/**
 * @brief Test if a page table entry refers to a read-only page.
 *
 * @param[in] pte The read-only pointer to the page table entry in question.
 * @return `true` if the referred page is read-only.
 * @return `false` if the referred page can be written to.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
bool __nonnull() is_readonly_pte(const pt_entry_t *pte);

/**
 * @brief Test if a page table entry is valid.
 *
 * @param[in] pte The read-only pointer to the page table entry in question.
 * @return `true` if the page table entry refers to a page in physical memory.
 * @return `false` if the page table entry does not refer to a page in the
 * physical memory.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
bool __nonnull() is_valid_pte(const pt_entry_t *pte);

/**
 * @brief Test if a page table entry refers to a dirty page.
 *
 * @param[in] pte The read-only pointer to the page table entry in question.
 * The page table entry must be valid.
 * @return `true` if the referred page has no backing outside the physical
 * memory.
 * @return `false` if the referred page is backed by something, e.g. a
 * corresponding swap page with the same content.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
bool __nonnull() is_dirty_pte(const pt_entry_t *pte);

/**
 * @brief Test if a page table entry refers to a swapped out page.
 *
 * @param[in] pte The read-only pointer to the page table entry in question.
 * @return `true` if the referred page is present only in swap.
 * @return `false` if the pte does not refer to any page or refers to a page in
 * physical memory.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
bool __nonnull() is_swapped_pte(const pt_entry_t *pte);

/**
 * @brief Get the frame number referred by this page table entry.
 *
 * @param[in] pte The read-only pointer to the page table entry in question.
 * @return The referenced frame number.
 *
 * @note STUDENT IMPLEMENTATION
 * @see pagetable.c
 */
pfn_t __nonnull() framenum_from_pte(const pt_entry_t *pte);


#endif /* __PAGETABLE_H__ */
