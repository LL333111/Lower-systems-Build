#ifndef __MALLOC369_H__
#define __MALLOC369_H__

#include "types.h"

/* malloc/free tracking functions */
i64 get_current_bytes_malloced();
i64 get_current_num_mallocs();
i64 get_num_mallocs();
i64 get_bytes_malloced();
bool is_leak_free(i32 num_mallocs_tol, i32 num_bytes_tol);

/**
 * @brief Allocate `size` bytes of memory, tracked by the csc369 subystem.
 * 
 * @param size[in] The size in bytes of memory to be allocated.
 * @return Null pointer if failed, pointer to `size` bytes if successful.
 * 
 * @see malloc369.c
 */
void *malloc369(size_t size);

/**
 * @brief Reallocate `size` bytes of memory allocated by malloc369() or
 * realloc369(), tracked by the csc369 subystem.
 * 
 * @param ptr[in] The pointer to bytes allocated previously by malloc369() or
 * realloc369().
 * @param size[in] The new size in bytes of memory requested.
 * @return Null pointer if failed, pointer to `size` bytes if successful.
 * Returned pointer could be different from `ptr`.
 * 
 * @see malloc369.c
 */
void *realloc369(void *ptr, size_t size);

/**
 * @brief Free memory allocated by malloc369() or realloc369(), tracked by the
 * csc369 subystem.
 * 
 * @param ptr[in] The pointer to bytes allocated previously by malloc369() or
 * realloc369().
 * 
 * @see malloc369.c
 */
void free369(void *ptr);

// malloc369 functions used in sim.c for initialization and teardown.
void init_csc369_malloc(bool verbose);
void destroy_csc369_malloc(void);

#endif /* _MALLOC369_H__ */
