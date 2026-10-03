/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "ki_kfunc.h"
#include "ki_fs_compat.h"
#include "ki_module.h"

#define KI_MODULE_LIST_MAX (64 * 1024)

static int ki_module_get_real(const char *key, char *value, size_t size)
{
	struct file *file;
	char *buf;
	loff_t pos = 0;
	ssize_t len;
	unsigned int count = 0;
	if (!key || !value || !size || strcmp(key, "count"))
		return -EINVAL;
	file = filp_open("/proc/modules", O_RDONLY | O_CLOEXEC, 0);
	if (IS_ERR(file))
		return PTR_ERR(file);
	buf = kzalloc(KI_MODULE_LIST_MAX, GFP_KERNEL);
	if (!buf) { filp_close(file, NULL); return -ENOMEM; }
	len = ki_kernel_read(file, buf, KI_MODULE_LIST_MAX - 1, &pos);
	filp_close(file, NULL);
	if (len < 0) { kfree(buf); return len; }
	while (len-- > 0) if (buf[len] == '\n') count++;
	snprintf(value, size, "%u", count);
	kfree(buf);
	return 0;
}
static int ki_module_get_real_key(unsigned int index, char *key, size_t size)
{
	if (!key || !size || index) return -ENOENT;
	strscpy(key, "count", size);
	return 0;
}

int ki_module_list_line(unsigned int index, char *line, size_t size)
{
	struct file *file;
	char *buf, *cursor, *next;
	loff_t pos = 0;
	ssize_t len;
	unsigned int current = 0;
	if (!line || !size) return -EINVAL;
	file = filp_open("/proc/modules", O_RDONLY | O_CLOEXEC, 0);
	if (IS_ERR(file)) return PTR_ERR(file);
	buf = kzalloc(KI_MODULE_LIST_MAX, GFP_KERNEL);
	if (!buf) { filp_close(file, NULL); return -ENOMEM; }
	len = ki_kernel_read(file, buf, KI_MODULE_LIST_MAX - 1, &pos);
	filp_close(file, NULL);
	if (len < 0) { kfree(buf); return len; }
	buf[len] = '\0';
	cursor = buf;
	while (cursor && *cursor) {
		next = strchr(cursor, '\n');
		if (next) *next++ = '\0';
		if (current++ == index) {
			strscpy(line, cursor, size);
			kfree(buf);
			return 0;
		}
		cursor = next;
	}
	kfree(buf);
	return -ENOENT;
}

struct ki_kfunc ki_module_kfunc = {
	.name = "module",
	.get_real = ki_module_get_real,
	.get_real_key = ki_module_get_real_key,
};
