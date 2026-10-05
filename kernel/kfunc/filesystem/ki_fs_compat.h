/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_FS_COMPAT_H
#define _KI_FS_COMPAT_H

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/version.h>

/* Mount flags are not exposed consistently by Android kernel headers. */
#ifndef MS_BIND
#define MS_BIND 4096
#endif

#ifndef MNT_DETACH
#define MNT_DETACH 2
#endif

static inline ssize_t ki_kernel_read(struct file *file, void *buf,
                                     size_t count, loff_t *pos)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	return kernel_read(file, buf, count, pos);
#else
	return kernel_read(file, *pos, buf, count);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0)
static inline ssize_t ki_fs_strscpy(char *dst, const char *src, size_t size)
{
	size_t len;

	if (!size)
		return -E2BIG;

	len = strlen(src);
	if (len >= size) {
		memcpy(dst, src, size - 1);
		dst[size - 1] = '\0';
		return -E2BIG;
	}

	memcpy(dst, src, len + 1);
	return (ssize_t)len;
}
#else
static inline ssize_t ki_fs_strscpy(char *dst, const char *src, size_t size)
{
	return strscpy(dst, src, size);
}
#endif

#endif
