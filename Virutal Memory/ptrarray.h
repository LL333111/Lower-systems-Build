/** @file ptrarray.h
 * @brief Generic Dynamic Array Utilities
 *
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * @author Nagata Parama Aptana
 *
 */
#ifndef __PTRARRAY_H__
#define __PTRARRAY_H__

#include "types.h"

/* Growable contiguous array containing non-null pointers, with configurable
 * growth pressure.
 */
typedef struct ptrarray_s ptrarray_t;

#define PTRARRAY_DEFAULT_PRESSURE 2.0

__returns_nonnull ptrarray_t *
ptrarray_init(u16 initial_capacity, f32 pressure);
__nonnull() void ptrarray_destroy(ptrarray_t *arr);

__nonnull() u16 ptrarray_get_capacity(const ptrarray_t *arr);
__nonnull() u16 ptrarray_get_size(const ptrarray_t *arr);
__nonnull() f32 ptrarray_get_pressure(const ptrarray_t *arr);

__nonnull() void ptrarray_append(ptrarray_t *arr, void *item);
__nonnull() void *ptrarray_remove(ptrarray_t *arr, void *item);
__nonnull() void ptrarray_clear(ptrarray_t *arr);

/**
 * @brief Represents a slice of a pointer array.
 */
typedef struct ptrarray_slice_s {
    void *const *const ptr; /** Pointer to the start of the slice. */
    const u16 len;          /** Length of the slice. */
} ptrarray_slice_t;

__nonnull() ptrarray_slice_t
ptrarray_get_slice(const ptrarray_t *arr, u16 begin, u16 end);


#endif // __PTRARRAY_H__
