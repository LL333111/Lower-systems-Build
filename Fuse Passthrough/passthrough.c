#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>

/**
 * Get host file system path.
 *
 * Returns false if the buffer is too small to fit the resulting path.
 */
static bool get_path(char *buf, size_t size, const char *path)
{
	const char *dir = (const char *)fuse_get_context()->private_data;
	assert(dir);

	size_t len = snprintf(buf, size, "%s", dir);
	if (len >= size)
		return false;

	// Remove trailing slash in dir (path always starts with a slash)
	if (dir[len - 1] == '/')
		--len;

	len += snprintf(buf + len, size - len, "%s", path);
	if (len >= size)
		return false;

	return true;
}

/**
 * Get file system statistics.
 */
static int passthrough_statfs(const char *path, struct statvfs *st)
{
	fprintf(stderr, "statfs(%s, %p)\n", path, (void *)st);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	return statvfs(abs_path, st) < 0 ? -errno : 0;
}

/**
 * Get file or directory attributes.
 */
static int passthrough_getattr(const char *path, struct stat *st)
{
	fprintf(stderr, "getattr(%s, %p)\n", path, (void *)st);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	// NOTE: using lstat() instead of stat() since FUSE doesn't expect this
	//       function to follow symlinks
	return lstat(abs_path, st) < 0 ? -errno : 0;
}

/**
 * Read a directory.
 */
static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
							   off_t offset, struct fuse_file_info *fi)
{
	fprintf(stderr, "readdir(%s, %p, %ld)\n", path, buf, (long)offset);
	(void)offset; // unused
	(void)fi;	  // unused

	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	DIR *dir = opendir(abs_path);
	if (!dir)
		return -errno;

	errno = 0; // will stay 0 if all readdir() calls succeed
	for (;;)
	{
		struct dirent *dentry = readdir(dir);
		if (!dentry)
			break;
		filler(buf, dentry->d_name, NULL, 0);
	}

	closedir(dir);
	return -errno;
}

/**
 * Read the target of a symbolic link.
 */
static int passthrough_readlink(const char *path, char *buf, size_t size)
{
	fprintf(stderr, "readlink(%s, %p, %lu)\n", path, (void *)buf, (unsigned long)size);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	ssize_t length = readlink(abs_path, buf, size - 1);
	if (length < 0)
		return -errno;
	buf[length] = '\0';
	return 0;
}

/**
 * Create a directory.
 */
static int passthrough_mkdir(const char *path, mode_t mode)
{
	fprintf(stderr, "mkdir(%s, %04o)\n", path, mode);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	return mkdir(abs_path, mode) < 0 ? -errno : 0;
}

/**
 * Remove a directory.
 */
static int passthrough_rmdir(const char *path)
{
	fprintf(stderr, "rmdir(%s)\n", path);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	return rmdir(abs_path) < 0 ? -errno : 0;
}

/**
 * Create a file.
 */
static int passthrough_create(const char *path, mode_t mode,
							  struct fuse_file_info *fi)
{
	fprintf(stderr, "create(%s, %04o)\n", path, mode);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	// NOTE: fi->flags already contains the right set of flags to pass to open()
	return open(abs_path, fi->flags, mode) < 0 ? -errno : 0;
}

/**
 * Remove a file.
 */
static int passthrough_unlink(const char *path)
{
	fprintf(stderr, "unlink(%s)\n", path);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	return unlink(abs_path) < 0 ? -errno : 0;
}

/**
 * Rename a file or directory.
 */
static int passthrough_rename(const char *from, const char *to)
{
	fprintf(stderr, "rename(%s, %s)\n", from, to);
	char from_abs_path[PATH_MAX], to_abs_path[PATH_MAX];
	if (!get_path(from_abs_path, sizeof(from_abs_path), from) ||
		!get_path(to_abs_path, sizeof(to_abs_path), to))
	{
		return -ENAMETOOLONG;
	}

	return rename(from_abs_path, to_abs_path) < 0 ? -errno : 0;
}

/**
 * Change the modification time of a file or directory.
 */
static int passthrough_utimens(const char *path, const struct timespec times[2])
{
	fprintf(stderr, "utimens(%s, %p)\n", path, (const void *)times);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	// NOTE: FUSE doesn't expect this function to follow symlinks
	return utimensat(AT_FDCWD, abs_path, times, AT_SYMLINK_NOFOLLOW) < 0 ? -errno : 0;
}

/**
 * Change the size of a file.
 */
static int passthrough_truncate(const char *path, off_t size)
{
	fprintf(stderr, "truncate(%s, %ld)\n", path, (long)size);
	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	return truncate(abs_path, size) < 0 ? -errno : 0;
}

/**
 * Read data from a file.
 */
static int passthrough_read(const char *path, char *buf, size_t size,
							off_t offset, struct fuse_file_info *fi)
{
	fprintf(stderr, "read(%s, %p, %lu, %ld)\n",
			path, (void *)buf, (unsigned long)size, (long)offset);
	(void)fi; // unused

	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	int fd = open(abs_path, O_RDONLY);
	if (fd < 0)
		return -errno;

	ssize_t result = pread(fd, buf, size, offset);
	close(fd);
	return result;
}

/**
 * Write data to a file.
 */
static int passthrough_write(const char *path, const char *buf, size_t size,
							 off_t offset, struct fuse_file_info *fi)
{
	fprintf(stderr, "write(%s, %p, %lu, %ld)\n",
			path, (const void *)buf, (unsigned long)size, (long)offset);
	(void)fi; // unused

	char abs_path[PATH_MAX];
	if (!get_path(abs_path, sizeof(abs_path), path))
		return -ENAMETOOLONG;

	int fd = open(abs_path, O_WRONLY);
	if (fd < 0)
		return -errno;

	ssize_t result = pwrite(fd, buf, size, offset);
	close(fd);
	return result;
}

static struct fuse_operations passthrough_ops = {
	.statfs = passthrough_statfs,
	.getattr = passthrough_getattr,
	.readdir = passthrough_readdir,
	.readlink = passthrough_readlink,
	.mkdir = passthrough_mkdir,
	.rmdir = passthrough_rmdir,
	.create = passthrough_create,
	.unlink = passthrough_unlink,
	.rename = passthrough_rename,
	.utimens = passthrough_utimens,
	.truncate = passthrough_truncate,
	.read = passthrough_read,
	.write = passthrough_write,
};

// We are using the existing option parsing infrastructure in FUSE.
// See fuse_opt.h in libfuse source code for details.

typedef struct passthrough_opts
{
	const char *dir;
	int help;
} passthrough_opts;

#define PASSTHROUGH_OPT(t, p) {t, offsetof(passthrough_opts, p), 1}

static const struct fuse_opt passthrough_opt_spec[] = {
	PASSTHROUGH_OPT("-h", help),
	PASSTHROUGH_OPT("--help", help),
	FUSE_OPT_END};

static const char *help_str = "\
Usage: %s source mountpoint [options]\n\
\n\
Mount a mirror of source directory at mountpoint. Use fusermount(1) to unmount.\n\
\n\
general options:\n\
    -o opt,[opt...]        mount options\n\
    -h   --help            print help\n\
";

// Callback for fuse_opt_parse()
static int opt_proc(void *data, const char *arg, int key, struct fuse_args *out)
{
	passthrough_opts *opts = (passthrough_opts *)data;
	(void)out; // unused

	if ((key == FUSE_OPT_KEY_NONOPT) && (opts->dir == NULL))
	{
		opts->dir = strdup(arg);
		return 0;
	}
	return 1;
}

static bool passthrough_opt_parse(struct fuse_args *args, passthrough_opts *opts)
{
	if (fuse_opt_parse(args, opts, passthrough_opt_spec, opt_proc) != 0)
	{
		return false;
	}

	// NOTE: printing to stderr to keep it consistent with FUSE
	if (opts->help)
	{
		fprintf(stderr, help_str, args->argv[0]);
		fuse_opt_add_arg(args, "-ho");
	}
	else if (!opts->dir)
	{
		fprintf(stderr, "Missing source directory\n");
		return false;
	}

	return true;
}

int main(int argc, char *argv[])
{
	passthrough_opts opts = {0}; // defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!passthrough_opt_parse(&args, &opts))
		return 1;

	struct stat st = {0};
	if (!opts.help && ((stat(opts.dir, &st) < 0) || !S_ISDIR(st.st_mode)))
	{
		fprintf(stderr, "Source path is not a directory\n");
		return 1;
	}

	// Get absolute path to the source directory so that it can still be
	// accessed when the file system runs in detached mode
	char *dir = realpath(opts.dir, NULL);
	if (!dir)
	{
		perror("realpath");
		return 1;
	}

	// Pass source directory as private data in the FUSE context
	return fuse_main(args.argc, args.argv, &passthrough_ops, dir);
}
