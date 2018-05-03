/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <string.h>
#include <errno.h>
#include <posix/pthread.h>
#include <posix/unistd.h>

union file_desc {
	struct fs_file_t file;
	struct fs_dir_t	dir;
};

struct posix_fs_desc {
	union file_desc desc;
	bool is_dir;
	bool used;
};

static struct posix_fs_desc desc_array[CONFIG_MAX_OPEN_FILES];

static struct fs_dirent fdirent;
static struct dirent pdirent;

#ifdef CONFIG_ERRNO
static inline void set_pthread_errno(int err)
{
	unsigned int key = irq_lock();

	((struct k_thread *)pthread_self())->errno_var = err;
	irq_unlock(key);
}
#else
#define set_pthread_errno(x) do { } while (0)
#endif

static int posix_fs_alloc_fd(void **ptr, bool is_dir)
{
	int fd;
	unsigned int key = irq_lock();

	for (fd = 0; fd < CONFIG_MAX_OPEN_FILES; fd++) {
		if (desc_array[fd].used == false) {
			if (is_dir) {
				*ptr = &desc_array[fd].desc.dir;
			} else {
				*ptr = &desc_array[fd].desc.file;
			}
			desc_array[fd].used = true;
			desc_array[fd].is_dir = is_dir;
			break;
		}
	}
	irq_unlock(key);

	if (fd >= CONFIG_MAX_OPEN_FILES) {
		return -1;
	}

	return fd;
}

static void posix_fs_free_fd(int fd)
{
	unsigned int key = irq_lock();

	desc_array[fd].used = false;
	desc_array[fd].is_dir = false;
	irq_unlock(key);
}

static void posix_fs_free_ptr(void *ptr)
{
	struct posix_fs_desc *desc = (struct posix_fs_desc *)ptr;
	unsigned int key = irq_lock();

	desc->used = false;
	desc->is_dir = false;
	irq_unlock(key);
}

static int posix_fs_get_ptr(int fd, void **ptr, bool is_dir)
{
	int rc = 0;
	unsigned int key;

	if (fd < 0 || fd >= CONFIG_MAX_OPEN_FILES) {
		return -1;
	}

	key = irq_lock();

	if ((desc_array[fd].used == true) &&
		(desc_array[fd].is_dir == is_dir)) {
		if (is_dir) {
			*ptr = &desc_array[fd].desc.dir;
		} else {
			*ptr = &desc_array[fd].desc.file;
		}
	} else {
		rc = -1;
	}
	irq_unlock(key);

	return rc;
}

/**
 * @brief Open a file.
 *
 * See IEEE 1003.1
 */
int open(const char *name, int flags)
{
	int rc, fd;
	struct fs_file_t *ptr = NULL;

	ARG_UNUSED(flags);

	fd = posix_fs_alloc_fd((void **)&ptr, false);
	if ((fd < 0) || (ptr == NULL)) {
		set_pthread_errno(ENFILE);
		return -1;
	}
	memset(ptr, 0, sizeof(struct fs_file_t));

	rc = fs_open(ptr, name);
	if (rc < 0) {
		posix_fs_free_fd(fd);
		set_pthread_errno(-rc);
		return -1;
	}

	return fd;
}

/**
 * @brief Close a file descriptor.
 *
 * See IEEE 1003.1
 */
int close(int fd)
{
	int rc;
	struct fs_file_t *ptr = NULL;

	if (posix_fs_get_ptr(fd, (void **)&ptr, false)) {
		set_pthread_errno(EBADF);
		return -1;
	}

	rc = fs_close(ptr);

	/* Free file ptr memory */
	posix_fs_free_fd(fd);

	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}

	return 0;
}

/**
 * @brief Write to a file.
 *
 * See IEEE 1003.1
 */
ssize_t write(int fd, char *buffer, unsigned int count)
{
	ssize_t rc;
	struct fs_file_t *ptr = NULL;

	if (posix_fs_get_ptr(fd, (void **)&ptr, false)) {
		set_pthread_errno(EBADF);
		return -1;
	}

	rc = fs_write(ptr, buffer, count);
	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}

	return rc;
}

/**
 * @brief Read from a file.
 *
 * See IEEE 1003.1
 */
ssize_t read(int fd, char *buffer, unsigned int count)
{
	ssize_t rc;
	struct fs_file_t *ptr = NULL;

	if (posix_fs_get_ptr(fd, (void **)&ptr, false)) {
		set_pthread_errno(EBADF);
		return -1;
	}

	rc = fs_read(ptr, buffer, count);
	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}

	return rc;
}

/**
 * @brief Move read/write file offset.
 *
 * See IEEE 1003.1
 */
int lseek(int fd, int offset, int whence)
{
	int rc;
	struct fs_file_t *ptr = NULL;

	if (posix_fs_get_ptr(fd, (void **)&ptr, false)) {
		set_pthread_errno(EBADF);
		return -1;
	}

	rc = fs_seek(ptr, offset, whence);
	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}

	return 0;
}

/**
 * @brief Open a directory stream.
 *
 * See IEEE 1003.1
 */
DIR *opendir(const char *dirname)
{
	int rc, fd;
	struct fs_dir_t *ptr = NULL;

	fd = posix_fs_alloc_fd((void **)&ptr, true);
	if ((fd < 0) || (ptr == NULL)) {
		set_pthread_errno(EMFILE);
		return NULL;
	}
	memset(ptr, 0, sizeof(struct fs_dir_t));

	rc = fs_opendir(ptr, dirname);
	if (rc < 0) {
		posix_fs_free_fd(fd);
		set_pthread_errno(-rc);
		return NULL;
	}

	return ptr;
}

/**
 * @brief Close a directory stream.
 *
 * See IEEE 1003.1
 */
int closedir(DIR *dirp)
{
	int rc;

	if (dirp == NULL) {
		set_pthread_errno(EBADF);
		return -1;
	}

	rc = fs_closedir(dirp);

	/* Free file ptr memory */
	posix_fs_free_ptr(dirp);

	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}

	return 0;
}

/**
 * @brief Read a directory.
 *
 * See IEEE 1003.1
 */
struct dirent *readdir(DIR *dirp)
{
	int rc;

	if (dirp == NULL) {
		set_pthread_errno(EBADF);
		return NULL;
	}

	rc = fs_readdir(dirp, &fdirent);
	if (rc < 0) {
		set_pthread_errno(-rc);
		return NULL;
	}

	memcpy(pdirent.d_name, fdirent.name, MAX_FILE_NAME); 
	return &pdirent;
}

/**
 * @brief Rename a file.
 *
 * See IEEE 1003.1
 */
int rename(const char *old, const char *new)
{
	int rc;

	rc = fs_rename(old, new);
	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}

	return 0;
}

/**
 * @brief Remove a directory entry.
 *
 * See IEEE 1003.1
 */
int unlink(const char *path)
{
	int rc;

	rc = fs_unlink(path);
	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}
	return 0;
}

/**
 * @brief Get file status.
 *
 * See IEEE 1003.1
 */
int stat(const char *path, struct stat *buf)
{
	int rc;
	struct fs_statvfs stat;

	if (buf == NULL) {
		set_pthread_errno(EBADF);
		return -1;
	}

	rc = fs_statvfs(path, &stat);
	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}

	buf->st_size = stat.f_bsize * stat.f_blocks;
	buf->st_blksize = stat.f_bsize;
	buf->st_blocks = stat.f_blocks;
	return 0;
}

/**
 * @brief Make a directory.
 *
 * See IEEE 1003.1
 */
int mkdir(const char *path, mode_t mode)
{
	int rc;

	ARG_UNUSED(mode);

	rc = fs_mkdir(path);
	if (rc < 0) {
		set_pthread_errno(-rc);
		return -1;
	}

	return 0;
}
