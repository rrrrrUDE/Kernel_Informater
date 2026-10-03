/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_FS_COMPAT_H
#define _KI_FS_COMPAT_H

#include <linux/fs.h>
#include <linux/version.h>

static inline ssize_t ki_kernel_read(struct file *file, void *buf,
					 size_t count, loff_t *pos)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	return kernel_read(file, *pos, buf, count);
#else
	return kernel_read(file, buf, count, pos);
#endif
}

#endif
