/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid, Angela Demke Brown
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2022 Angela Demke Brown
 */

/**
 * CSC369 Assignment 4 - exfs command line options parser header file.
 */

#pragma once

#include <stdbool.h>

#include <fuse_opt.h>

/** exfs command line options. */
typedef struct exfs_opts {
    /** exfs image file path. */
    const char* img_path;
    /** Print help and exit. FUSE option. */
    int help;

} exfs_opts;

/**
 * Parse exfs command line options.
 *
 * @param args  pointer to 'struct fuse_args' with the program arguments.
 * @param args  pointer to the options struct that receives the result.
 * @return      true on success; false on failure.
 */
bool exfs_opt_parse(struct fuse_args* args, exfs_opts* opts);