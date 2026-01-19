#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "khash.h"
#include "types.h"

/* Need 2^63 bytes malloced before these will overflow as 
 * signed types, and having signs makes the math safer
 * if the accounting is wrong.
 */
i64 num_mallocs;    /* Total number of malloc369 calls */
i64 num_reallocs;   /* Total number of realloc369 calls */
i64 num_frees;      /* Total number of free369 calls */
i64 bytes_malloced; /* Total number of bytes malloced */
i64 bytes_freed;    /* Total number of bytes freed */

#define GB            1024*1024*1024L /* 1 GB, signed long type */
#define MALLOC369_MAX 2*GB            /* maximum dynamically allocated memory */

static bool verbose;

/* Add some status bits to the 'size' stored in the malloc map */
#define FREED   0x8000000000000000 	  /* if set, ptr has been freed already */

KHASH_MAP_INIT_INT64(ptrmap, size_t)
khash_t(ptrmap) *malloc_map = NULL;
		
void *
malloc369(size_t size)
{
	i64 signed_size;
	
	/* Check if allocating 'size' bytes would overflow our tracking.
	 * On teach.cs servers, this isn't needed because the underlying 
	 * malloc() will fail long before we overflow a signed long.
	 * But we might run this on other systems, so check anyway.
	 */ 
	if (size >= MALLOC369_MAX) {
		printf("malloc369 - size must be less than %ld, requested %lu\n",
		       MALLOC369_MAX, size);
		return NULL;
	}
	if (((size_t)bytes_malloced + size) > MALLOC369_MAX) {
		printf("malloc369 - total bytes allocated must be less than %ld, "
		       "with current request for %lu bytes, total would be %lu\n",
		       MALLOC369_MAX, size, ((size_t)bytes_malloced + size));
		return NULL;
	}

	signed_size = (i64)size;
	void * m = malloc(signed_size);
	if (m == NULL) {
		/* nothing allocated, nothing to track */
		return m;
	}

	num_mallocs++;
	bytes_malloced += signed_size;

	/* Record the ptr for later free tracking */
	i32 ret;
	khiter_t k = kh_put(ptrmap, malloc_map, (size_t)m, &ret);
	assert(ret >= 0);
	if (ret == 0 && verbose) { /* key was present and not deleted */
		printf("malloc369 - malloc returned reused ptr\n");
	}
	kh_value(malloc_map, k) = signed_size;
	
	return m;
}

void *
realloc369(void * ptr, size_t new_size)
{
	/* Check if allocating 'size' bytes would overflow our tracking.
	 * On teach.cs servers, this isn't needed because the underlying
	 * malloc() will fail long before we overflow a signed long.
	 * But we might run this on other systems, so check anyway.
	 */
	if (new_size >= MALLOC369_MAX) {
		printf("realloc369 - size must be less than %ld, requested %lu\n",
			   MALLOC369_MAX, new_size);
		return NULL;
	}

	const khiter_t k = kh_get(ptrmap, malloc_map, (size_t)ptr);
	const bool is_missing = (k == kh_end(malloc_map));

	size_t old_size = 0;
	if (!is_missing) {
		old_size = kh_value(malloc_map, k);
	} else {
		if (verbose) {
			printf("realloc369 - trying to free a ptr that is "
			"not in our map!\n");
		}
		// free(ptr); /* Should abort if our map is correct. */
		return NULL;
	}

	/* Check for double-free.
	 */
	if (old_size & FREED) {
		if (verbose) {
			printf("realloc of already freed ptr %p detected!\n",
				   ptr);
		}
		// free(ptr); /* Should abort if our check is correct */
		return NULL;
	}

	if (new_size > old_size) {
		const size_t added_size = new_size - old_size;
		if ((size_t)bytes_malloced + added_size > MALLOC369_MAX) {
			printf("realloc369 - total bytes allocated must be less than %ld, "
				"with current request for %lu bytes, total would be %lu\n",
				MALLOC369_MAX, added_size, ((size_t)bytes_malloced + added_size));
			return NULL;
		}
	}

	const i64 signed_size = (i64)new_size;
	void *r = realloc(ptr, signed_size);
	if (r == NULL)
		return NULL; // nothing changed

	num_reallocs += 1;
	bytes_malloced += new_size;
	bytes_freed += old_size;

	kh_value(malloc_map, k) |= FREED; // free old pointer
	i32 ret;
	const khiter_t kp = kh_put(ptrmap, malloc_map, (size_t)r, &ret);
	assert(ret >= 0);
	if (ret == 0 && verbose) { /* key was present and not deleted */
		printf("realloc369 - realloc returned reused ptr\n");
	}
	kh_value(malloc_map, kp) = signed_size;

	return r;
}

void
free369(void *ptr)
{
    size_t size = 0;
	bool is_missing = true;
    khiter_t k;
		
	if (ptr == NULL) {
		/* Ok to free(NULL) but we don't want to count that as  
		 * matching an actual malloc.
		 */
		free(ptr);
		return;
	}

	k = kh_get(ptrmap, malloc_map, (size_t)ptr);
	is_missing = (k == kh_end(malloc_map));
	
	/* Get the size and check if we are trying to free an address that 
	 * we didn't get from malloc. 
	 */

	if (!is_missing) {
		size = kh_value(malloc_map, k);
	} else {
		if (verbose) {
			printf("free369 - trying to free a ptr that is "
				      "not in our map!\n");
		}
		free(ptr); /* Should abort if our map is correct. */
		return;
	}

	
	/* Check for double-free.
	 */
	if (size & FREED) {
		if (verbose) {
			printf("free of already freed ptr %p detected!\n", 
			       ptr);
		}
		free(ptr); /* Should abort if our check is correct */
		return;
	}
	
	/* Count one more free of size bytes */
	assert(size != 0);
	assert(size < LONG_MAX);
	assert(num_mallocs - num_frees > 0);
	num_frees++;
	assert((bytes_malloced - bytes_freed) >= (i64)size);
	bytes_freed += size;

	/* Fill freed memory with 0xee to help detect use-after-free bugs. */
	/* Why 0xee? Because (a) filling with 0xff can look like -1 which might
	 * be misleading, and (b) filling with a hex-word like '0xdead' 
	 * requires either an assumption that malloc'd sizes are always even
	 * or more complicated code to check if size is even or odd. 
	 * Depending on how you look at things you may see memory containing
	 * 0xee in different ways. For example, when viewed as:
	 *     char:     0xee (1 byte) = -18
	 *     unsigned char: 0xee (1 byte) = 238      
	 *     Viewed as an int:   0xeeeeeeee (4 bytes) = -286331154
	 *     Viewed as unsigned: 0xeeeeeeee (4 bytes) = 4008636142
	 *     Viewed as long: 0xeeeeeeeeeeeeeeee (8 bytes) = -1229782938247303442
	 *     Viewed as unsigned long: 0xeeeeeeeeeeeeeeee (8 bytes) = 17216961135462248174
	 *     Viewed as ptr: 0xeeeeeeeeeeeeeeee (8 bytes) = 0xeeeeeeeeeeeeeeee
	 *
	 * Looking at memory in hex, or as (void *) type in gdb will make it
	 * easy to spot the 'freed memory chunk' pattern. 
	 */

	char *region = (char *)ptr;
	for (size_t i = 0; i < size; i++) {
		region[i] = 0xee;
	}
	free(ptr);
	kh_value(malloc_map, k) |= FREED;
	
}

void
init_csc369_malloc(bool verb)
{
    malloc_map = kh_init(ptrmap);
	verbose = verb;
	num_mallocs = 0;
	bytes_malloced = 0;
	num_frees = 0;
	bytes_freed = 0;
}

void
destroy_csc369_malloc(void)
{
	kh_destroy(ptrmap, malloc_map);
	malloc_map = NULL;
}

i64
get_current_bytes_malloced()
{
	assert(bytes_malloced >= bytes_freed);
	return (bytes_malloced - bytes_freed);
}

i64
get_current_num_mallocs()
{
	assert(num_mallocs >= num_frees);
	return (num_mallocs - num_frees);
}

i64
get_num_mallocs()
{
	return num_mallocs;
}

i64
get_bytes_malloced()
{
	return bytes_malloced;
}

/* Pass in 'tolerance' for number of mallocs and bytes malloc'd that we 
 * won't consider a leak.
 */
bool
is_leak_free(i32 num_mallocs_tol, i32 num_bytes_tol)
{
	if (get_current_bytes_malloced() > num_bytes_tol ||
	    get_current_num_mallocs() > num_mallocs_tol) {
		return false;
	} else {
		return true;
	}
}
