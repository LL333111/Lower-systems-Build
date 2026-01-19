/** @file ptrarray.c
 * @brief Generic Dynamic Array Implementation for Pointers
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ptrarray.h"
#include "types.h"
#include "malloc369.h"

struct ptrarray_s {
    u16 size;
    u16 capacity;
    f32 pressure;
    void **items;
};

__returns_nonnull ptrarray_t *
ptrarray_init(u16 initial_capacity, f32 pressure)
{
    assert(initial_capacity > 0);
    assert(pressure > 1.0);

    ptrarray_t *const pa = malloc369(sizeof(ptrarray_t));
    if (pa == NULL) {
        perror("ptarray out of memory");
        abort();
    }

    pa->items = malloc369(sizeof(void *) * initial_capacity);
    if (pa->items == NULL) {
        perror("ptarray out of memory");
        free369(pa);
        abort();
    }

    pa->size = 0;
    pa->capacity = initial_capacity;
    pa->pressure = pressure;
    memset(pa->items, 0, sizeof(void *) * initial_capacity);
    return pa;
}

__nonnull() void
ptrarray_destroy(ptrarray_t *arr)
{
    assert(arr->capacity > 0);
    free369(arr->items);
    free369(arr);
}

__nonnull() u16
ptrarray_get_capacity(const ptrarray_t *arr)
{
    assert(arr->capacity > 0);
    return arr->capacity;
}

__nonnull() u16
ptrarray_get_size(const ptrarray_t *arr)
{
    return arr->size;
}

__nonnull() f32
ptrarray_get_pressure(const ptrarray_t *arr)
{
    return arr->pressure;
}

__nonnull() void
ptrarray_append(ptrarray_t *arr, void *item)
{
    assert(arr->capacity > 0);
    assert(arr->size <= arr->capacity);
    assert(arr->pressure > 1.0);

    if (arr->size == arr->capacity) {
        const size_t new_capacity = ceilf(arr->pressure * arr->capacity);
        assert(new_capacity > arr->capacity);

        void *const new_items =
            realloc369(arr->items, sizeof(void *) * new_capacity);
        if (new_items == NULL) {
            perror("ptrarray out of memory");
            free369(arr->items);
            ptrarray_destroy(arr);
            abort();
        }

        arr->items = new_items;
        arr->capacity = new_capacity;
    }

    assert(arr->size < arr->capacity);
    arr->items[arr->size] = item;
    arr->size += 1;
}

__nonnull() void *
ptrarray_remove(ptrarray_t *arr, void *item)
{
    assert(arr->capacity > 0);
    assert(arr->size <= arr->capacity);

    u16 i = 0;
    while (i < arr->size && arr->items[i] != item)
        i += 1;

    if (i == arr->size)
        return NULL;    // not found

    void *const ret = arr->items[i];
    arr->size -= 1;

    for (;i < arr->size; i += 1)
        arr->items[i] = arr->items[i + 1];

    assert(arr->capacity > 0);
    assert(arr->size <= arr->capacity);
    return ret;
}

__nonnull() void
ptrarray_clear(ptrarray_t *arr)
{
    memset(arr->items, 0, sizeof(void *) * arr->size);
    arr->size = 0;
}

__nonnull() ptrarray_slice_t
ptrarray_get_slice(const ptrarray_t *arr, u16 begin, u16 end)
{
    assert(begin < end);
    assert(begin < arr->size);
    return (ptrarray_slice_t) {
        .ptr = &arr->items[begin],
        .len = end < arr->size ? end : arr->size,
    };
}
