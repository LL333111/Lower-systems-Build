/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Sherwin Okhowat, Jackson Tsang, Kuei (Jack) Sun
 *
 * CSC369 Assignment 4 - exfs driver implementation.
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2025 University of Toronto
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "bitmap.h"
#include "exfs.h"
#include "fs_ctx.h"
#include "map.h"
#include "options.h"
#include "util.h"

// NOTE: All path arguments are absolute paths within the exfs file system and
//  start with a '/' that corresponds to the exfs root directory.
//
//  For example, if exfs is mounted at "/tmp/my_userid", the path to a
//  file at "/tmp/my_userid/file" (as seen by the OS) will be
//  passed to FUSE callbacks as "/file".
//
//  Paths to directories (except for the root directory - "/") do not end in a
//  trailing '/'. For example, "/tmp/my_userid/dir/" will be passed to
//  FUSE callbacks as "/dir".

/**
 * Initialize the file system.
 *
 * Called when the file system is mounted. NOTE: we are not using the FUSE
 * init() callback since it doesn't support returning errors. This function must
 * be called explicitly before fuse_main().
 *s
 * @param fs    file system context to initialize.
 * @param opts  command line options.
 * @return      true on success; false on failure.
 */
static bool exfs_init(fs_ctx *fs, exfs_opts *opts)
{
	size_t size;
	void *image;

	// Nothing to initialize if only printing help
	if (opts->help)
	{
		return true;
	}

	// Map the disk image file into memory
	image = map_file(opts->img_path, EXFS_BLOCK_SIZE, &size);
	if (image == NULL)
	{
		return false;
	}

	return fs_ctx_init(fs, image, size);
}

/**
 * Cleanup the file system.
 *
 * Called when the file system is unmounted. Must cleanup all the resources
 * created in exfs_init().
 */
static void exfs_destroy(void *ctx)
{
	fs_ctx *fs = (fs_ctx *)ctx;
	if (fs->image)
	{
		munmap(fs->image, fs->size);
		fs_ctx_destroy(fs);
	}
}

/** Get file system context. */
static fs_ctx *get_fs(void)
{
	return (fs_ctx *)fuse_get_context()->private_data;
}

/* Returns the inode number for the element at the end of the path
 * if it exists.  If there is any error, return -1.
 * Possible errors include:
 *   - The path is not an absolute path
 *   - An element on the path cannot be found
 */
static int path_lookup(const char *path, exfs_ino_t *ino)
{
	// Check for absolute path
	if (path[0] != '/')
	{
		fprintf(stderr, "Not an absolute path\n");
		return -ENOSYS;
	}

	// Handle root directory case
	if (strcmp(path, "/") == 0)
	{
		*ino = EXFS_ROOT_INO;
		return 0;
	}

	// Sanity-check path length
	if (strlen(path) >= EXFS_PATH_MAX)
	{
		return -ENAMETOOLONG;
	}

	// Extract the name
	const char *name = path + 1;
	if (*name == '\0' || strlen(name) >= EXFS_NAME_MAX)
	{
		return -ENOENT;
	}

	// Look up entries in the root directory
	fs_ctx *ctx = get_fs();
	exfs_inode *root_inode = &ctx->itable[EXFS_ROOT_INO];

	size_t dir_entry_size = sizeof(exfs_dentry);
	size_t total_dir_entries = root_inode->i_size / dir_entry_size;
	size_t entries_seen = 0;
	size_t entries_per_block = EXFS_BLOCK_SIZE / dir_entry_size;

	for (uint32_t ext_idx = 0;
		 ext_idx < root_inode->i_num_extents && entries_seen < total_dir_entries;
		 ext_idx++)
	{
		exfs_extent *extent = &root_inode->extents[ext_idx];

		if (extent->length == 0)
		{
			continue;
		}

		for (uint32_t block_offset = 0;
			 block_offset < extent->length && entries_seen < total_dir_entries;
			 block_offset++)
		{
			exfs_blk_t block_num = extent->start_block + block_offset;
			uint8_t *block_ptr = (uint8_t *)ctx->image + block_num * EXFS_BLOCK_SIZE;
			exfs_dentry *dentries = (exfs_dentry *)block_ptr;

			for (size_t idx_in_block = 0;
				 idx_in_block < entries_per_block && entries_seen < total_dir_entries;
				 idx_in_block++, entries_seen++)
			{
				exfs_dentry *d = &dentries[idx_in_block];
				if (d->ino == EXFS_INO_MAX || d->name[0] == '\0')
				{
					continue;
				}
				if (strncmp(d->name, name, EXFS_NAME_MAX) == 0)
				{
					*ino = d->ino;
					return 0;
				}
			}
		}
	}

	return -ENOENT;
}

/**
 * Calculate a 32-bit checksum over a slice of the buffer, defined by
 * [offset, offset + count). This is similar to buffer[offset:offset+count]
 * in Python.
 *
 * The checksum is computed by XORing every 4-byte (32-bit) word in the
 * specified slice. The data is treated as a sequence of 4-byte chunks:
 *
 *   If the slice does not begin on a 4-byte boundary, the leading bytes
 *     are padded with zeros before being XORed.
 *   If the slice ends in the middle of a 4-byte word, the remaining bytes
 *     are also padded with zeros.
 *
 * In other words, the input is conceptually copied into a temporary buffer
 * whose length is rounded up to the next multiple of 4, and all missing
 * bytes are zero-filled. The checksum is then the XOR of all 32-bit words
 * in that padded buffer.
 *
 * Errors: none
 *
 * @param size    Total number of bytes in the buffer.
 * @param buffer  Pointer to the data buffer.
 * @param offset  Starting byte position within the buffer.
 * @param count   Number of bytes to include in the checksum.
 * @return        32-bit XOR checksum of the specified slice.
 */

static uint32_t calculate_checksum(size_t size, const uint8_t *buffer,
								   off_t offset, size_t count)
{
	assert(size >= offset + count);
	uint64_t start = (uint64_t)offset;
	uint64_t end = start + (uint64_t)count;
	uint64_t aligned_start = start - (start % 4);
	uint64_t aligned_end = (end + 3) & ~((uint64_t)3);
	uint32_t sum = 0;

	for (uint64_t word_base = aligned_start; word_base < aligned_end; word_base += 4)
	{
		uint32_t word = 0;
		for (uint64_t byte = 0; byte < 4; byte++)
		{
			uint64_t current_pos = word_base + byte;
			uint8_t value = 0;
			if (current_pos >= start && current_pos < end)
			{
				value = buffer[current_pos];
			}
			word |= ((uint32_t)value) << (byte * 8);
		}
		sum ^= word;
	}

	return sum;
}

static uint32_t checksum_word(const uint8_t bytes[4])
{
	return (uint32_t)bytes[0] |
		   ((uint32_t)bytes[1] << 8) |
		   ((uint32_t)bytes[2] << 16) |
		   ((uint32_t)bytes[3] << 24);
}

static uint32_t checksum_extents(fs_ctx *fs, const exfs_inode *inode)
{
	if (inode->i_size == 0 || inode->i_num_extents == 0)
	{
		return 0;
	}

	uint32_t sum = 0;
	uint8_t word_buf[4] = {0};
	size_t buf_len = 0;
	uint64_t processed = 0;
	uint64_t total = inode->i_size;

	for (uint32_t ext_idx = 0; ext_idx < inode->i_num_extents && processed < total; ext_idx++)
	{
		const exfs_extent *ext = &inode->extents[ext_idx];
		if (ext->length == 0)
		{
			continue;
		}
		for (uint32_t blk_off = 0; blk_off < ext->length && processed < total; blk_off++)
		{
			exfs_blk_t blkno = ext->start_block + blk_off;
			const uint8_t *block_ptr = (const uint8_t *)fs->image + (uint64_t)blkno * EXFS_BLOCK_SIZE;
			size_t chunk = EXFS_BLOCK_SIZE;
			uint64_t remain = total - processed;
			if ((uint64_t)chunk > remain)
			{
				chunk = (size_t)remain;
			}
			for (size_t i = 0; i < chunk; i++)
			{
				word_buf[buf_len++] = block_ptr[i];
				if (buf_len == 4)
				{
					sum ^= checksum_word(word_buf);
					buf_len = 0;
				}
			}
			processed += chunk;
		}
	}

	if (buf_len > 0)
	{
		while (buf_len < 4)
		{
			word_buf[buf_len++] = 0;
		}
		sum ^= checksum_word(word_buf);
	}

	return sum;
}

static uint32_t inode_checksum(fs_ctx *fs, const exfs_inode *inode)
{
	if (inode->i_size == 0)
	{
		return 0;
	}
	if (inode->i_flags & EXFS_FLAG_INLINE)
	{
		size_t count = (size_t)inode->i_size;
		if (count > EXFS_INLINE_SIZE)
		{
			count = EXFS_INLINE_SIZE;
		}
		return calculate_checksum(EXFS_INLINE_SIZE, inode->inline_data, 0, count);
	}
	return checksum_extents(fs, inode);
}

/**
 * Get file system statistics.
 *
 * Implements the statvfs() system call. See "man 2 statvfs" for details.
 * The f_bfree and f_bavail fields should be set to the same value.
 * The f_ffree and f_favail fields should be set to the same value.
 * The following fields can be ignored: f_fsid, f_flag.
 * All num_to_keep fields are required.
 *
 * Errors: none
 *
 * @param path  path to any file in the file system. Can be ignored.
 * @param st    pointer to the struct statvfs that receives the result.
 * @return      0 on success; -errno on error.
 */
static int exfs_statfs(const char *path, struct statvfs *st)
{
	(void)path; // unused
	fs_ctx *fs = get_fs();
	exfs_superblock *sb = fs->sb; /* Get ptr to superblock from context */

	memset(st, 0, sizeof(*st));
	st->f_bsize = EXFS_BLOCK_SIZE;	/* Filesystem block size */
	st->f_frsize = EXFS_BLOCK_SIZE; /* Fragment size */

	// The rest of required fields are filled based on the information
	// stored in the superblock.
	st->f_blocks = sb->sb_num_blocks;  /* Size of fs in f_frsize units */
	st->f_bfree = sb->sb_free_blocks;  /* Number of free blocks */
	st->f_bavail = sb->sb_free_blocks; /* Free blocks for unpriv users */
	st->f_files = sb->sb_num_inodes;   /* Number of inodes */
	st->f_ffree = sb->sb_free_inodes;  /* Number of free inodes */
	st->f_favail = sb->sb_free_inodes; /* Free inodes for unpriv users */

	st->f_namemax = EXFS_NAME_MAX; /* Maximum filename length */

	return 0;
}

/**
 * Get file or directory attributes.
 *
 * Implements the lstat() system call. See "man 2 lstat" for details.
 * The following fields can be ignored: st_dev, st_ino, st_uid, st_gid, st_rdev,
 *                                      st_blksize, st_atim, st_ctim.
 * All num_to_keep fields are required.
 *
 * NOTE: the st_blocks field is measured in 512-byte units (disk sectors);
 *       it should include any metadata blocks that are allocated to the
 *       inode (for exfs, that is the indirect block).
 *
 * NOTE2: the st_mode field must be set correctly for files and directories.
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * @param path  path to a file or directory.
 * @param st    pointer to the struct stat that receives the result.
 * @return      0 on success; -errno on error;
 */
static int exfs_getattr(const char *path, struct stat *st)
{
	if (strlen(path) >= EXFS_PATH_MAX)
		return -ENAMETOOLONG;
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));

	// Look the up the inode for the given path
	exfs_ino_t ino;
	int result = path_lookup(path, &ino);
	// If there was an error, return it
	if (result != 0)
	{
		return result;
	}
	// Otherwise, fill in the required fields
	exfs_inode *inode = &fs->itable[ino];
	st->st_ino = ino;
	st->st_mode = inode->i_mode;
	st->st_nlink = inode->i_nlink;
	st->st_size = inode->i_size;
	st->st_blocks = inode->i_blocks * (EXFS_BLOCK_SIZE / 512);
	st->st_mtim = inode->i_mtime;

	return 0;
}

/**
 * Read a directory.
 *
 * Implements the readdir() system call. Should call filler(buf, name, NULL, 0)
 * for each directory entry. See fuse.h in libfuse source code for details.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a filler() call failed).
 *
 * @param path    path to the directory.
 * @param buf     buffer that receives the result.
 * @param filler  function that needs to be called for each directory entry.
 *                Pass 0 as offset (4th argument). 3rd argument can be NULL.
 * @param offset  unused.
 * @param fi      unused.
 * @return        0 on success; -errno on error.
 */

static int fill_dir_entries(fs_ctx *fs, exfs_inode *dir, void *buf, fuse_fill_dir_t filler)
{
	size_t entry_size = sizeof(exfs_dentry);
	size_t total_entries = dir->i_size / entry_size;
	size_t entries_seen = 0;
	size_t entries_per_block = EXFS_BLOCK_SIZE / entry_size;

	for (uint32_t ext_idx = 0; ext_idx < dir->i_num_extents && entries_seen < total_entries; ext_idx++)
	{
		exfs_extent *extent = &dir->extents[ext_idx];

		// Skip empty extents
		if (extent->length == 0)
		{
			continue;
		}

		for (uint32_t block_idx = 0; block_idx < extent->length && entries_seen < total_entries; block_idx++)
		{
			exfs_blk_t block_num = extent->start_block + block_idx;
			uint8_t *block_ptr = (uint8_t *)fs->image + block_num * EXFS_BLOCK_SIZE;
			exfs_dentry *dentries = (exfs_dentry *)block_ptr;

			for (size_t i = 0; i < entries_per_block && entries_seen < total_entries; i++, entries_seen++)
			{
				exfs_dentry *d = &dentries[i];
				if (d->ino == EXFS_INO_MAX || d->name[0] == '\0')
				{
					continue;
				}
				if (filler(buf, d->name, NULL, 0) != 0)
				{
					return -ENOMEM;
				}
			}
		}
	}

	return 0;
}

static int exfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{
	(void)offset; // unused
	(void)fi;	  // unused

	// check for absolute path
	if (path[0] != '/')
	{
		fprintf(stderr, "Not an absolute path\n");
		return -ENOSYS;
	}
	// get the root inode and start searching for the entry
	fs_ctx *fs = get_fs();
	exfs_inode *root = &fs->itable[EXFS_ROOT_INO];
	int rv = fill_dir_entries(fs, root, buf, filler);
	return rv;
}

/**
 * Create a file.
 *
 * Implements the open()/create() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to create.
 * @param mode  file mode bits.
 * @param fi    unused.
 * @return      0 on success; -errno on error.
 */

static exfs_dentry *dir_find_free_entry(fs_ctx *fs, exfs_inode *dir)
{
	size_t ents_per_block = EXFS_BLOCK_SIZE / sizeof(exfs_dentry);
	size_t num_entries = dir->i_size / sizeof(exfs_dentry);
	if (num_entries == 0 || dir->i_num_extents == 0)
	{
		return NULL;
	}
	size_t seen = 0;

	for (uint32_t e = 0; e < dir->i_num_extents && seen < num_entries; e++)
	{
		exfs_extent *ext = &dir->extents[e];

		if (ext->length == 0)
		{
			continue;
		}

		for (uint32_t b = 0; b < ext->length && seen < num_entries; b++)
		{
			exfs_blk_t blk_no = ext->start_block + b;
			uint8_t *blk_base = (uint8_t *)fs->image + blk_no * EXFS_BLOCK_SIZE;
			exfs_dentry *block_entries = (exfs_dentry *)blk_base;

			for (size_t i = 0; i < ents_per_block && seen < num_entries; i++, seen++)
			{
				exfs_dentry *cur = &block_entries[i];
				if (cur->ino == EXFS_INO_MAX || cur->name[0] == '\0')
				{
					return cur;
				}
			}
		}
	}

	return NULL;
}

static int dir_grow_block(fs_ctx *fs, exfs_inode *dir, exfs_dentry **slot)
{
	uint32_t new_db_index;

	if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, &new_db_index) != 0)
	{
		return -ENOSPC;
	}

	exfs_blk_t new_blkno = (exfs_blk_t)new_db_index;

	int merged = 0;

	if (dir->i_num_extents > 0)
	{
		exfs_extent *last = &dir->extents[dir->i_num_extents - 1];
		if (last->length > 0 && last->start_block + last->length == new_blkno)
		{
			last->length += 1;
			merged = 1;
		}
	}

	if (!merged)
	{
		if (dir->i_num_extents >= EXFS_NUM_EXTENTS)
		{
			bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, new_db_index);
			return -ENOSPC;
		}

		exfs_extent *fresh = &dir->extents[dir->i_num_extents++];
		fresh->start_block = new_blkno;
		fresh->length = 1;
	}
	dir->i_blocks += 1;
	dir->i_size += EXFS_BLOCK_SIZE;
	fs->sb->sb_free_blocks--;

	uint8_t *raw = (uint8_t *)fs->image + new_blkno * EXFS_BLOCK_SIZE;
	memset(raw, 0, EXFS_BLOCK_SIZE);

	exfs_dentry *dentries = (exfs_dentry *)raw;
	size_t entries_per_block = EXFS_BLOCK_SIZE / sizeof(exfs_dentry);

	for (size_t i = 0; i < entries_per_block; i++)
	{
		dentries[i].ino = EXFS_INO_MAX;
		dentries[i].name[0] = '\0';
	}

	if (slot != NULL)
	{
		*slot = &dentries[0];
	}

	return 0;
}

static int exfs_create(const char *path, mode_t mode,
					   struct fuse_file_info *fi)
{
	(void)fi; // unused
	assert(S_ISREG(mode));
	fs_ctx *fs = get_fs();
	exfs_superblock *sb = fs->sb;
	exfs_inode *root = &fs->itable[EXFS_ROOT_INO];

	if (path[0] != '/')
	{
		return -EINVAL;
	}
	if (strlen(path) >= EXFS_PATH_MAX)
	{
		return -ENAMETOOLONG;
	}
	const char *basename = path + 1;
	if (*basename == '\0' || strchr(basename, '/') != NULL)
	{
		return -EINVAL;
	}
	if (strlen(basename) >= EXFS_NAME_MAX)
	{
		return -ENAMETOOLONG;
	}

	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) != 0)
	{
		return -errno;
	}

	exfs_ino_t existing;
	if (path_lookup(path, &existing) == 0)
	{
		return -EEXIST;
	}

	exfs_dentry *free_entry = dir_find_free_entry(fs, root);
	uint32_t inode_index;
	if (bitmap_alloc(fs->ibmap, sb->sb_num_inodes, &inode_index) != 0)
	{
		return -ENOSPC;
	}

	if (free_entry == NULL)
	{
		int grow_res = dir_grow_block(fs, root, &free_entry);
		if (grow_res != 0)
		{
			bitmap_free(fs->ibmap, sb->sb_num_inodes, inode_index);
			return grow_res;
		}
	}

	exfs_inode *inode = &fs->itable[inode_index];
	memset(inode, 0, sizeof(*inode));
	inode->i_mode = S_IFREG | (mode & 0777);
	inode->i_nlink = 1;
	inode->i_blocks = 0;
	inode->i_flags = EXFS_FLAG_INLINE;
	inode->i_size = 0;
	inode->i_mtime = now;
	inode->i_checksum = 0;
	inode->i_num_extents = 0;

	memset(free_entry, 0, sizeof(*free_entry));
	free_entry->ino = (exfs_ino_t)inode_index;
	strncpy(free_entry->name, basename, EXFS_NAME_MAX - 1);
	free_entry->name[EXFS_NAME_MAX - 1] = '\0';

	sb->sb_free_inodes--;
	root->i_mtime = now;

	return 0;
}

/**
 * Remove a file.
 *
 * Implements the unlink() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path  path to the file to remove.
 * @return      0 on success; -errno on error.
 */

static exfs_dentry *dir_lookup_entry(fs_ctx *fs, exfs_inode *dir, const char *name)
{
	size_t entries_per_block = EXFS_BLOCK_SIZE / sizeof(exfs_dentry);
	size_t total_entries = dir->i_size / sizeof(exfs_dentry);
	size_t visited = 0;

	for (uint32_t ext_idx = 0; ext_idx < dir->i_num_extents && visited < total_entries; ext_idx++)
	{
		exfs_extent *extent = &dir->extents[ext_idx];
		if (extent->length == 0)
		{
			continue;
		}
		for (uint32_t blk_offset = 0; blk_offset < extent->length && visited < total_entries; blk_offset++)
		{
			exfs_blk_t block_no = extent->start_block + blk_offset;
			exfs_dentry *dentries = (exfs_dentry *)((uint8_t *)fs->image + block_no * EXFS_BLOCK_SIZE);
			for (size_t entry_idx = 0; entry_idx < entries_per_block && visited < total_entries; entry_idx++, visited++)
			{
				exfs_dentry *entry = &dentries[entry_idx];
				if (entry->ino == EXFS_INO_MAX || entry->name[0] == '\0')
				{
					continue;
				}
				if (strncmp(entry->name, name, EXFS_NAME_MAX) == 0)
				{
					return entry;
				}
			}
		}
	}

	return NULL;
}
static int exfs_unlink(const char *path)
{
	fs_ctx *fs = get_fs();
	exfs_superblock *sb = fs->sb;
	exfs_inode *root = &fs->itable[EXFS_ROOT_INO];
	// Necessary checks
	if (path[0] != '/')
	{
		return -EINVAL;
	}
	if (strcmp(path, "/") == 0)
	{
		return -EPERM;
	}
	if (strlen(path) >= EXFS_PATH_MAX)
	{
		return -ENAMETOOLONG;
	}
	// More necessary checks
	const char *name = path + 1;
	if (*name == '\0' || strchr(name, '/') != NULL || strlen(name) >= EXFS_NAME_MAX)
	{
		return -EINVAL;
	}
	// Check if the file exists
	exfs_ino_t target_ino;
	int lookup_res = path_lookup(path, &target_ino);
	if (lookup_res != 0)
	{
		return lookup_res;
	}
	// Find the directory entry for the file
	exfs_dentry *entry = dir_lookup_entry(fs, root, name);
	if (entry == NULL)
	{
		return -ENOENT;
	}
	// Free data blocks if the file is not inline
	exfs_inode *inode = &fs->itable[target_ino];
	uint32_t blocks_n = 0;
	if (!(inode->i_flags & EXFS_FLAG_INLINE))
	{
		for (uint32_t i = 0; i < inode->i_num_extents; i++)
		{
			exfs_extent *ext = &inode->extents[i];
			if (ext->length == 0)
			{
				continue;
			}
			for (uint32_t blk = 0; blk < ext->length; blk++)
			{
				bitmap_free(fs->dbmap, sb->sb_num_blocks, ext->start_block + blk);
				blocks_n++;
			}
		}
		inode->i_num_extents = 0;
	}
	if (blocks_n > 0)
	{
		inode->i_blocks = 0;
		sb->sb_free_blocks += blocks_n;
	}

	bitmap_free(fs->ibmap, sb->sb_num_inodes, target_ino);
	sb->sb_free_inodes++;
	memset(inode, 0, sizeof(*inode));
	entry->ino = EXFS_INO_MAX;
	entry->name[0] = '\0';
	// Update the modification time of the root directory
	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) == 0)
	{
		root->i_mtime = now;
	}

	return 0;
}

/**
 * Change the modification time of a file or directory.
 *
 * Implements the utimensat() system call. See "man 2 utimensat" for details.
 *
 * NOTE: You only need to implement the setting of modification time (mtime).
 *       Timestamp modifications are not recursive.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists.
 *
 * Errors: none
 *
 * @param path   path to the file or directory.
 * @param times  timestamps array. See "man 2 utimensat" for details.
 * @return       0 on success; -errno on failure.
 */
static int exfs_utimens(const char *path, const struct timespec times[2])
{
	if (strlen(path) >= EXFS_PATH_MAX)
	{
		return -ENAMETOOLONG;
	}

	const struct timespec *in_times = times;
	struct timespec temp_times[2];

	if (in_times == NULL)
	{
		if (clock_gettime(CLOCK_REALTIME, &temp_times[0]) != 0)
		{
			return -errno;
		}
		temp_times[1] = temp_times[0];
		in_times = temp_times;
	}

	if (in_times[1].tv_nsec == UTIME_OMIT)
	{
		return 0;
	}

	fs_ctx *fs = get_fs();
	exfs_ino_t found_ino;
	int lookup_rc = path_lookup(path, &found_ino);
	if (lookup_rc != 0)
	{
		return lookup_rc;
	}

	exfs_inode *node = &fs->itable[found_ino];
	struct timespec final_mtime;

	if (in_times[1].tv_nsec == UTIME_NOW)
	{
		if (clock_gettime(CLOCK_REALTIME, &final_mtime) != 0)
		{
			return -errno;
		}
	}
	else
	{
		final_mtime = in_times[1];
	}

	node->i_mtime = final_mtime;
	return 0;
}

/**
 * Change the size of a file.
 *
 * Implements the truncate() system call. Supports both extending and shrinking.
 * If the file is extended, the new uninitialized range at the end must be
 * filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *   EFBIG   write would exceed the maximum file size.
 *
 * @param path  path to the file to set the size.
 * @param size  new file size in bytes.
 * @return      0 on success; -errno on error.
 */

static bool is_valid_data_block(fs_ctx *fs, exfs_blk_t blkno)
{
	return blkno >= fs->sb->sb_data_region && blkno < fs->sb->sb_num_blocks;
}


static uint32_t count_free_run(fs_ctx *fs, exfs_blk_t start_block, uint32_t max_len)
{
	if (!is_valid_data_block(fs, start_block))
	{
		return 0;
	}
	if (start_block >= fs->sb->sb_num_blocks)
	{
		return 0;
	}

	exfs_blk_t end = fs->sb->sb_num_blocks;
	uint32_t count = 0;
	for (exfs_blk_t blk = start_block; blk < end && count < max_len; blk++)
	{
		if (bitmap_isset(fs->dbmap, fs->sb->sb_num_blocks, blk))
		{
			break;
		}
		count++;
	}
	return count;
}

static int find_best_free_run(fs_ctx *fs, uint32_t max_len,
								  exfs_blk_t *start_out, uint32_t *len_out)
{
	exfs_blk_t first = fs->sb->sb_data_region;
	exfs_blk_t end = fs->sb->sb_num_blocks;
	exfs_blk_t current_start = EXFS_BLK_UNASSIGNED;
	uint32_t current_len = 0;
	exfs_blk_t best_start = EXFS_BLK_UNASSIGNED;
	uint32_t best_len = 0;

	for (exfs_blk_t blk = first; blk < end; blk++)
	{
		if (!bitmap_isset(fs->dbmap, fs->sb->sb_num_blocks, blk))
		{
			if (current_len == 0)
			{
				current_start = blk;
			}
			current_len++;
			if (current_len == max_len)
			{
				best_start = current_start;
				best_len = current_len;
				break;
			}
		}
		else
		{
			if (current_len > best_len)
			{
				best_start = current_start;
				best_len = current_len;
			}
			current_len = 0;
		}
	}

	if (current_len > best_len)
	{
		best_start = current_start;
		best_len = current_len;
	}

	if (best_len == 0)
	{
		return -ENOSPC;
	}

	if (best_len > max_len)
	{
		best_len = max_len;
	}

	*start_out = best_start;
	*len_out = best_len;
	return 0;
}

static int append_extent_range(fs_ctx *fs, exfs_inode *inode,
							   exfs_blk_t start, uint32_t length)
{
	if (length == 0)
	{
		return 0;
	}
	exfs_extent *last = NULL;
	if (inode->i_num_extents > 0)
	{
		last = &inode->extents[inode->i_num_extents - 1];
	}
	bool merges_with_last = false;
	if (last != NULL && last->length > 0 &&
		last->start_block + last->length == start)
	{
		merges_with_last = true;
	}
	if (!merges_with_last && inode->i_num_extents >= EXFS_NUM_EXTENTS)
	{
		return -EFBIG;
	}
	if (fs->sb->sb_free_blocks < length)
	{
		return -ENOSPC;
	}

	for (uint32_t i = 0; i < length; i++)
	{
		exfs_blk_t blk = start + i;
		if (!is_valid_data_block(fs, blk) ||
			bitmap_isset(fs->dbmap, fs->sb->sb_num_blocks, blk))
		{
			return -EIO;
		}
	}

	for (uint32_t i = 0; i < length; i++)
	{
		exfs_blk_t blk = start + i;
		bitmap_set(fs->dbmap, fs->sb->sb_num_blocks, blk, true);
		uint8_t *ptr = (uint8_t *)fs->image + (uint64_t)blk * EXFS_BLOCK_SIZE;
		memset(ptr, 0, EXFS_BLOCK_SIZE);
	}

	if (merges_with_last)
	{
		last->length += length;
	}
	else
	{
		exfs_extent *slot = &inode->extents[inode->i_num_extents++];
		slot->start_block = start;
		slot->length = length;
	}

	inode->i_blocks += length;
	fs->sb->sb_free_blocks -= length;
	return 0;
}

static int free_last_block(fs_ctx *fs, exfs_inode *inode)
{
	if (inode->i_num_extents == 0 || inode->i_blocks == 0)
	{
		return -EIO;
	}

	exfs_extent *last = &inode->extents[inode->i_num_extents - 1];
	while (inode->i_num_extents > 0 && last->length == 0)
	{
		inode->i_num_extents--;
		if (inode->i_num_extents == 0)
		{
			return -EIO;
		}
		last = &inode->extents[inode->i_num_extents - 1];
	}

	exfs_blk_t blkno = last->start_block + last->length - 1;
	bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, blkno);
	fs->sb->sb_free_blocks++;
	inode->i_blocks -= 1;
	last->length -= 1;
	if (last->length == 0)
	{
		memset(last, 0, sizeof(*last));
		inode->i_num_extents--;
	}
	return 0;
}

static int ensure_blocks_for_size(fs_ctx *fs, exfs_inode *inode, uint64_t target_size)
{
	if (target_size == 0)
	{
		return 0;
	}
	uint64_t needed = (target_size + EXFS_BLOCK_SIZE - 1) / EXFS_BLOCK_SIZE;
	if ((uint64_t)inode->i_blocks >= needed)
	{
		return 0;
	}
	uint32_t original_blocks = inode->i_blocks;
	int fail_rc = 0;
	while ((uint64_t)inode->i_blocks < needed)
	{
		uint32_t remaining = (uint32_t)(needed - inode->i_blocks);
		uint32_t extend_len = 0;
		exfs_extent *last = (inode->i_num_extents > 0) ? &inode->extents[inode->i_num_extents - 1] : NULL;
		if (last != NULL && last->length > 0)
		{
			exfs_blk_t candidate = last->start_block + last->length;
			extend_len = count_free_run(fs, candidate, remaining);
			if (extend_len > 0)
			{
				int rc = append_extent_range(fs, inode, candidate, extend_len);
				if (rc != 0)
				{
					fail_rc = rc;
					goto ensure_fail;
				}
				continue;
			}
		}

		exfs_blk_t run_start;
		uint32_t run_len;
		int rc = find_best_free_run(fs, remaining, &run_start, &run_len);
		if (rc != 0)
		{
			fail_rc = rc;
			goto ensure_fail;
		}
		rc = append_extent_range(fs, inode, run_start, run_len);
		if (rc != 0)
		{
			fail_rc = rc;
			goto ensure_fail;
		}
	}
	return 0;

ensure_fail:
	while (inode->i_blocks > original_blocks)
	{
		if (free_last_block(fs, inode) != 0)
		{
			break;
		}
	}
	return (fail_rc != 0) ? fail_rc : -ENOSPC;
}

static bool map_block(const exfs_inode *inode, uint64_t file_block_idx, exfs_blk_t *blkno);

static int shrink_to_size(fs_ctx *fs, exfs_inode *inode, uint64_t target_size)
{
	uint64_t needed = 0;
	if (target_size > 0)
	{
		needed = (target_size + EXFS_BLOCK_SIZE - 1) / EXFS_BLOCK_SIZE;
	}
	while ((uint64_t)inode->i_blocks > needed)
	{
		int rc = free_last_block(fs, inode);
		if (rc != 0)
		{
			return rc;
		}
	}
	if (target_size == 0 || needed == 0)
	{
		return 0;
	}
	if ((target_size % EXFS_BLOCK_SIZE) == 0)
	{
		return 0;
	}
	exfs_blk_t blkno;
	if (!map_block(inode, (target_size - 1) / EXFS_BLOCK_SIZE, &blkno))
	{
		return -EIO;
	}
	size_t tail = (size_t)(target_size % EXFS_BLOCK_SIZE);
	uint8_t *ptr = (uint8_t *)fs->image + (uint64_t)blkno * EXFS_BLOCK_SIZE;
	memset(ptr + tail, 0, EXFS_BLOCK_SIZE - tail);
	return 0;
}

static int zero_extent_range(fs_ctx *fs, exfs_inode *inode, uint64_t start, uint64_t end)
{
	if (end <= start)
	{
		return 0;
	}
	uint64_t pos = start;
	while (pos < end)
	{
		exfs_blk_t blkno;
		if (!map_block(inode, pos / EXFS_BLOCK_SIZE, &blkno))
		{
			return -EIO;
		}
		size_t offset = (size_t)(pos % EXFS_BLOCK_SIZE);
		size_t chunk = EXFS_BLOCK_SIZE - offset;
		uint64_t remain = end - pos;
		if ((uint64_t)chunk > remain)
		{
			chunk = (size_t)remain;
		}
		uint8_t *ptr = (uint8_t *)fs->image + (uint64_t)blkno * EXFS_BLOCK_SIZE;
		memset(ptr + offset, 0, chunk);
		pos += chunk;
	}
	return 0;
}

static void restore_inline_inode(exfs_inode *inode, const uint8_t *snapshot,
									  uint32_t checksum, uint64_t size)
{
	inode->i_flags |= EXFS_FLAG_INLINE;
	inode->i_num_extents = 0;
	inode->i_blocks = 0;
	memset(inode->extents, 0, sizeof(inode->extents));
	memcpy(inode->inline_data, snapshot, EXFS_INLINE_SIZE);
	inode->i_size = size;
	inode->i_checksum = checksum;
}

static int convert_inline_to_extents(fs_ctx *fs, exfs_inode *inode)
{
	if (!(inode->i_flags & EXFS_FLAG_INLINE))
	{
		return 0;
	}

	uint8_t data_copy[EXFS_INLINE_SIZE];
	memcpy(data_copy, inode->inline_data, sizeof(data_copy));
	uint64_t bytes = inode->i_size;
	uint32_t old_checksum = inode->i_checksum;

	inode->i_flags &= ~EXFS_FLAG_INLINE;
	inode->i_num_extents = 0;
	inode->i_blocks = 0;
	memset(inode->extents, 0, sizeof(inode->extents));

	int rc = ensure_blocks_for_size(fs, inode, bytes);
	if (rc != 0)
	{
		restore_inline_inode(inode, data_copy, old_checksum, bytes);
		return rc;
	}

	uint64_t copied = 0;
	while (copied < bytes)
	{
		exfs_blk_t blkno;
		if (!map_block(inode, copied / EXFS_BLOCK_SIZE, &blkno))
		{
			rc = -EIO;
			break;
		}
		size_t offset = (size_t)(copied % EXFS_BLOCK_SIZE);
		size_t chunk = EXFS_BLOCK_SIZE - offset;
		uint64_t remain = bytes - copied;
		if ((uint64_t)chunk > remain)
		{
			chunk = (size_t)remain;
		}
		uint8_t *ptr = (uint8_t *)fs->image + (uint64_t)blkno * EXFS_BLOCK_SIZE;
		memcpy(ptr + offset, data_copy + copied, chunk);
		copied += chunk;
	}

	if (rc != 0)
	{
		shrink_to_size(fs, inode, 0);
		restore_inline_inode(inode, data_copy, old_checksum, bytes);
		return rc;
	}

	inode->i_checksum = old_checksum;
	return 0;
}

static int exfs_truncate(const char *path, off_t size)
{
	if (size < 0)
	{
		return -EINVAL;
	}

	exfs_ino_t ino;
	int rc = path_lookup(path, &ino);
	if (rc != 0)
	{
		return rc;
	}

	fs_ctx *fs = get_fs();
	exfs_inode *inode = &fs->itable[ino];
	uint64_t new_size = (uint64_t)size;
	uint64_t old_size = inode->i_size;
	uint8_t inline_snapshot[EXFS_INLINE_SIZE];
	uint32_t inline_checksum = 0;
	uint64_t inline_size_copy = 0;
	bool truncate_restore_inline = false;

	if ((inode->i_flags & EXFS_FLAG_INLINE) && new_size <= EXFS_INLINE_SIZE)
	{
		if (new_size > old_size)
		{
			size_t extra = (size_t)(new_size - old_size);
			memset(inode->inline_data + old_size, 0, extra);
		}
		else if (new_size < old_size)
		{
			size_t removed = (size_t)(old_size - new_size);
			memset(inode->inline_data + new_size, 0, removed);
		}
		inode->i_size = new_size;
	}
	else
	{
		if (inode->i_flags & EXFS_FLAG_INLINE)
		{
			memcpy(inline_snapshot, inode->inline_data, EXFS_INLINE_SIZE);
			inline_checksum = inode->i_checksum;
			inline_size_copy = inode->i_size;
			truncate_restore_inline = true;
			rc = convert_inline_to_extents(fs, inode);
			if (rc != 0)
			{
				return rc;
			}
			old_size = inode->i_size;
		}

		if (new_size > old_size)
		{
			rc = ensure_blocks_for_size(fs, inode, new_size);
			if (rc != 0)
			{
				goto truncate_fail;
			}
			rc = zero_extent_range(fs, inode, old_size, new_size);
			if (rc != 0)
			{
				goto truncate_fail;
			}
		}
		else if (new_size < old_size)
		{
			rc = shrink_to_size(fs, inode, new_size);
			if (rc != 0)
			{
				goto truncate_fail;
			}
		}

		inode->i_size = new_size;
	}

	inode->i_checksum = inode_checksum(fs, inode);

	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) == 0)
	{
		inode->i_mtime = now;
	}

	return 0;

truncate_fail:
	if (truncate_restore_inline)
	{
		shrink_to_size(fs, inode, 0);
		restore_inline_inode(inode, inline_snapshot, inline_checksum, inline_size_copy);
	}
	return rc;
}

/**
 * Read data from a file.
 *
 * Implements the pread() system call. Must return exactly the number of bytes
 * requested except on EOF (end of file). Reads from file ranges that have not
 * been written to must return ranges filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path    path to the file to read from.
 * @param buf     pointer to the buffer that receives the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to read from.
 * @param fi      unused.
 * @return        number of bytes read on success; 0 if offset is beyond EOF;
 *                -errno on error.
 */

static bool map_block(const exfs_inode *inode, uint64_t file_block_idx, exfs_blk_t *blkno)
{
	uint64_t start = 0;

	for (uint32_t i = 0; i < inode->i_num_extents; i++)
	{
		exfs_extent *ext = (exfs_extent *)&inode->extents[i];

		if (ext->length == 0)
		{
			continue;
		}
		uint64_t range = ext->length;
		if (file_block_idx < start + range)
		{
			uint64_t offset_in_extent = file_block_idx - start;
			*blkno = ext->start_block + (exfs_blk_t)offset_in_extent;
			return true;
		}
		start += range;
	}
	return false;
}

static int exfs_read(const char *path, char *buf, size_t size, off_t offset,
					 struct fuse_file_info *fi)
{
	(void)fi; // unused
	fs_ctx *fs = get_fs();
	exfs_ino_t ino;
	// Necessary checks
	if (path_lookup(path, &ino) != 0)
	{
		return -ENOENT;
	}
	exfs_inode *inode = &fs->itable[ino];
	if ((size == 0) || (offset >= (off_t)inode->i_size))
	{
		return 0;
	}
	size_t n_read = inode->i_size - offset;
	if (size < n_read)
	{
		n_read = size;
	}
	// If the file is stored inline, copy directly from the inline data
	if (inode->i_flags & EXFS_FLAG_INLINE)
	{
		memcpy(buf, inode->inline_data + offset, n_read);
		return (int)n_read;
	}
	// If the file is not stored inline, read from the blocks by using extents
	memset(buf, 0, n_read);
	uint64_t block_idx = (uint64_t)offset / EXFS_BLOCK_SIZE;
	size_t block_offset = (size_t)offset % EXFS_BLOCK_SIZE;
	size_t chunk = EXFS_BLOCK_SIZE - block_offset;
	if (chunk > n_read)
	{
		chunk = n_read;
	}

	exfs_blk_t actual_block;
	if (map_block(inode, block_idx, &actual_block))
	{
		uint8_t *blkptr = (uint8_t *)fs->image + (uint64_t)actual_block * EXFS_BLOCK_SIZE;
		memcpy(buf, blkptr + block_offset, chunk);
	}

	return (int)n_read;
}

/**
 * Write data to a file.
 *
 * Implements the pwrite() system call. Must return exactly the number of bytes
 * requested except on error. If the offset is beyond EOF (end of file), the
 * file must be extended. If the write creates a "hole" of uninitialized data,
 * the new uninitialized range must filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *   EFBIG   write would exceed the maximum file size
 *
 * @param path    path to the file to write to.
 * @param buf     pointer to the buffer containing the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to write to.
 * @param fi      unused.
 * @return        number of bytes written on success; -errno on error.
 */
static int exfs_write(const char *path, const char *buf, size_t size,
					  off_t offset, struct fuse_file_info *fi)
{
	(void)fi; // unused
	fs_ctx *fs = get_fs();

	if (size == 0)
	{
		return 0;
	}
	if (offset < 0)
	{
		return -EINVAL;
	}

	exfs_ino_t ino;
	int rc = path_lookup(path, &ino);
	if (rc != 0)
	{
		return rc;
	}
	exfs_inode *inode = &fs->itable[ino];
	uint8_t inline_snapshot[EXFS_INLINE_SIZE];
	uint32_t inline_checksum = 0;
	uint64_t inline_size = 0;
	bool restore_inline = false;

	uint64_t off64 = (uint64_t)offset;
	uint64_t len64 = (uint64_t)size;
	if (len64 > 0 && off64 > UINT64_MAX - len64)
	{
		return -EFBIG;
	}
	uint64_t write_end = off64 + len64;
	uint64_t fs_cap = (uint64_t)fs->sb->sb_num_blocks * EXFS_BLOCK_SIZE;
	if (write_end > fs_cap)
	{
		return -EFBIG;
	}

	bool storing_inline = (inode->i_flags & EXFS_FLAG_INLINE) != 0;
	bool stays_inline = (write_end <= EXFS_INLINE_SIZE);

	if (storing_inline && stays_inline)
	{
		if (off64 > inode->i_size)
		{
			uint64_t hole = off64 - inode->i_size;
			memset(inode->inline_data + inode->i_size, 0, (size_t)hole);
		}

		memcpy(inode->inline_data + off64, buf, size);
		if (write_end > inode->i_size)
		{
			inode->i_size = write_end;
		}
	}
	else
	{
		if (storing_inline)
		{
			memcpy(inline_snapshot, inode->inline_data, EXFS_INLINE_SIZE);
			inline_checksum = inode->i_checksum;
			inline_size = inode->i_size;
			restore_inline = true;
			rc = convert_inline_to_extents(fs, inode);
			if (rc != 0)
			{
				return rc;
			}
		}

		rc = ensure_blocks_for_size(fs, inode, write_end);
		if (rc != 0)
		{
			goto write_fail;
		}

		if (off64 > inode->i_size)
		{
			rc = zero_extent_range(fs, inode, inode->i_size, off64);
			if (rc != 0)
			{
				goto write_fail;
			}
		}

		uint64_t pos = off64;
		size_t left = size;
		const char *src_ptr = buf;
		while (left > 0)
		{
			exfs_blk_t blkno;
			if (!map_block(inode, pos / EXFS_BLOCK_SIZE, &blkno))
			{
				rc = -EIO;
				goto write_fail;
			}
			size_t block_off = (size_t)(pos % EXFS_BLOCK_SIZE);
			size_t chunk = EXFS_BLOCK_SIZE - block_off;
			if (chunk > left)
			{
				chunk = left;
			}
			uint8_t *dst = (uint8_t *)fs->image + (uint64_t)blkno * EXFS_BLOCK_SIZE + block_off;
			memcpy(dst, src_ptr, chunk);
			pos += chunk;
			src_ptr += chunk;
			left -= chunk;
		}

		if (write_end > inode->i_size)
		{
			inode->i_size = write_end;
		}
	}

	inode->i_checksum = inode_checksum(fs, inode);

	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) == 0)
	{
		inode->i_mtime = now;
	}

	return (int)size;

write_fail:
	if (restore_inline)
	{
		shrink_to_size(fs, inode, 0);
		restore_inline_inode(inode, inline_snapshot, inline_checksum, inline_size);
	}
	return rc;
}

static struct fuse_operations exfs_ops = {
	.destroy = exfs_destroy,
	.statfs = exfs_statfs,
	.getattr = exfs_getattr,
	.readdir = exfs_readdir,
	.create = exfs_create,
	.unlink = exfs_unlink,
	.utimens = exfs_utimens,
	.truncate = exfs_truncate,
	.read = exfs_read,
	.write = exfs_write,
};

int main(int argc, char *argv[])
{
	exfs_opts opts = {0}; // defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!exfs_opt_parse(&args, &opts))
		return 1;

	fs_ctx fs = {0};
	if (!exfs_init(&fs, &opts))
	{
		fprintf(stderr, "Failed to mount the file system\n");
		return 1;
	}

	return fuse_main(args.argc, args.argv, &exfs_ops, &fs);
}
