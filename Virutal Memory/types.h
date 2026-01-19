/** @file types.h
 * @brief Types Used in The Simulator
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
 *
 */

#ifndef __TYPES_H__
#define __TYPES_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

// basic data types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;
//
typedef i64 vpn_t; // use 36 bits
#define VPN_MASK (0xFFFFFFFFF)
typedef i64 pfn_t; // use 48 bits
#define INVALID_FRAME (0xFFFFFFFFFFFF)

typedef u64 paddr_t;  /* physical address is 64 bits */
typedef i64 vaddr_t; /* virtual address is 48 bits */

typedef u16 asid_t;
#define INVALID_ASID ((asid_t) UINT16_MAX)
#define ASID_MASK INVALID_ASID

typedef i64 off_t;

struct tlb_config;
struct mp_config;


#endif // __TYPES_H__
