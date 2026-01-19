/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Sherwin Okhowat, Jackson Tsang, Alexey Khrabrov, Karen Reid, Angela Demke Brown
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2025 University of Toronto
 */

/**
 * CSC369 Assignment 4 - exfs types, constants, and data structures header file.
 */

#pragma once

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <limits.h>

/**
 * exfs block size in bytes. You are not allowed to change this value.
 *
 * The block size is the unit of space allocation. Each file (and directory)
 * must occupy an integral number of blocks. Each of the file systems metadata
 * partitions, e.g. superblock, inode/block bitmaps, inode table (but not an
 * individual inode) must also occupy an integral number of blocks.
 */
#define EXFS_BLOCK_SIZE 4096
#define EXFS_SECTORS_PER_BLOCK (EXFS_BLOCK_SIZE / 512)
#define EXFS_NUM_EXTENTS 26
#define EXFS_INLINE_SIZE 208

/** Block number (block pointer) type. */
typedef uint32_t exfs_blk_t;

/** Inode number type. */
typedef uint32_t exfs_ino_t;

/** Magic value that can be used to identify an exfs image. */
#define EXFS_MAGIC 0xC5C369A4C5C369A4ul

/* exfs has simple layout
 *   Block 0: superblock
 *   Block 1: inode bitmap
 *   Block 2: data bitmap
 *   Block 3: start of inode table
 *   First data block after inode table
 */

#define EXFS_SB_BLKNUM 0
#define EXFS_IMAP_BLKNUM 1
#define EXFS_DMAP_BLKNUM 2
#define EXFS_ITBL_BLKNUM 3

/** exfs superblock. */

typedef struct exfs_superblock
{
    uint64_t sb_magic;         /* Must match EXFS_MAGIC. */
    uint64_t sb_size;          /* File system size in bytes. */
    uint32_t sb_num_inodes;    /* Total number of inodes (set by mkfs) */
    uint32_t sb_free_inodes;   /* Number of available inodes */
    exfs_blk_t sb_num_blocks;  /* File system size in blocks */
    exfs_blk_t sb_free_blocks; /* Number of available blocks in file sys */
    exfs_blk_t sb_data_region; /* First block after inode table */
} exfs_superblock;

/* Superblock must fit into a single disk sector */
static_assert(sizeof(exfs_superblock) <= EXFS_BLOCK_SIZE, "superblock is too large");

/** exfs extent */
typedef struct exfs_extent
{
    uint32_t start_block;
    uint32_t length;
} exfs_extent;

/** Bit 0 of the flag is inline */
#define EXFS_FLAG_INLINE 0x1

/** exfs inode. */
typedef struct exfs_inode
{
    /* File mode. */
    mode_t i_mode;

    /*
     * Reference count (number of hard links).
     *
     * Each file is referenced by its parent directory. Each directory is
     * referenced by its parent directory, itself (via "."), and each
     * subdirectory (via ".."). The "parent directory" of the root directory
     * is the root directory itself.
     */
    uint32_t i_nlink;

    /* File size in exfs file system blocks */
    exfs_blk_t i_blocks;

    /* File specific flags. */
    uint32_t i_flags;

    /* File size in bytes. */
    uint64_t i_size;

    /*
     * Last modification timestamp.
     *
     * Must be updated when the file (or directory) is created, written to,
     * or its size changes. Use the clock_gettime() function from time.h
     * with the CLOCK_REALTIME clock; see "man 3 clock_gettime" for details.
     */
    struct timespec i_mtime;

    /* Checksum of the file data (inline or extents). */
    uint32_t i_checksum;

    /* Number of allocated extents. */
    uint32_t i_num_extents;

    /**
     * File data storage.
     *
     * For small files, data is stored directly in inline_data to save space and improve performance.
     * For larger files, an array of 'exfs_extent' is used to map file data to disk blocks.
     */
    union
    {
        uint8_t inline_data[EXFS_INLINE_SIZE];
        exfs_extent extents[EXFS_NUM_EXTENTS];
    };
} exfs_inode;

/** A single block must fit an integral number of inodes */
static_assert(EXFS_BLOCK_SIZE % sizeof(exfs_inode) == 0, "invalid inode size");

/**
 *  Since we only have 1 inode bitmap block, there can be at most
 *  EXFS_BLOCK_SIZE * bits_per_byte inodes in the file system.
 */
#define EXFS_INO_MAX (EXFS_BLOCK_SIZE * CHAR_BIT)

/**
 * Define the inode number for the root directory.
 */
#define EXFS_ROOT_INO 0

/** The root inode must be in the first block of the inode table. */
static_assert(EXFS_ROOT_INO < (EXFS_BLOCK_SIZE / sizeof(exfs_inode)), "invalid root inode number");

/**
 *  Since we only have 1 data bitmap block, there can be at most
 *  EXFS_BLOCK_SIZE * bits_per_byte blocks in the file system.
 */
#define EXFS_BLK_MAX (EXFS_BLOCK_SIZE * CHAR_BIT)

/**
 *  Since we have a fixed metadata layout, there must be at least
 *  5 blocks in the file system:
 *  superblock, inode bitmap, data bitmap, inode table, root directory data blk
 */
#define EXFS_BLK_MIN 5

/**
 * Data block numbers must be > EXFS_ITBL_BLKNUM and < EXFS_BLK_MAX
 * for any EXFS file system, but we define 0 as the expected value to use
 * for an unassigned data block number in an inode or extent.
 */
#define EXFS_BLK_UNASSIGNED 0

/** Maximum file name (path component) length. Includes the null terminator. */
#define EXFS_NAME_MAX 252

/** Maximum file path length. Includes the null terminator. */
#define EXFS_PATH_MAX _POSIX_PATH_MAX

/** Fixed size directory entry structure. */
typedef struct exfs_dentry
{
    exfs_ino_t ino;            /* Inode number. */
    char name[EXFS_NAME_MAX];  /* File name. A null-terminated string. */
} exfs_dentry;

static_assert(sizeof(exfs_dentry) == 256, "invalid dentry size");