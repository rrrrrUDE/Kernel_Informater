/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "ki_kfunc.h"
#include "ki_fs_compat.h"
#include "ki_module.h"

#define KI_MODULE_MAX_ARGS 16
#define KI_MODULE_LIST_MAX (64 * 1024)

static int ki_module_run(char *const argv[])
{
	char *envp[] = {"HOME=/", "PATH=/system/bin:/system/xbin:/vendor/bin", NULL};
	if (!argv || !argv[0])
		return -EINVAL;
	return call_usermodehelper("/system/bin/toybox", argv, envp, UMH_WAIT_PROC);
}

static int ki_module_insmod(const char *spec)
{
	char *buf, *cursor, *token;
	char *argv[KI_MODULE_MAX_ARGS + 2];
	int argc = 0, ret;

	if (!spec || !*spec)
		return -EINVAL;
	buf = kstrdup(spec, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	cursor = buf;
	while ((token = strsep(&cursor, " \\t")) != NULL) {
		if (!*token)
			continue;
		if (argc >= KI_MODULE_MAX_ARGS) {
			ret = -E2BIG;
			goto out;
		}
		argv[argc++] = token;
	}
	if (!argc) {
		ret = -EINVAL;
		goto out;
	}
	memmove(&argv[1], &argv[0], argc * sizeof(argv[0]));
	argv[0] = "insmod";
	argv[argc + 1] = NULL;
	ret = ki_module_run(argv);
out:
	kfree(buf);
	return ret;
}

static int ki_module_rmmod(const char *name)
{
	char *argv[] = {"rmmod", (char *)name, NULL};
	if (!name || !*name || strchr(name, '/'))
		return -EINVAL;
	return ki_module_run(argv);
}

static int ki_module_func_set(const char *key, const char *value)
{
	if (!key || !value || !*value)
		return -EINVAL;
	if (!strcmp(key, "insmod"))
		return ki_module_insmod(value);
	if (!strcmp(key, "rmmod"))
		return ki_module_rmmod(value);
	return -EINVAL;
}
static int ki_module_func_unset(const char *key) { return key && *key ? -EOPNOTSUPP : -EINVAL; }
static int ki_module_func_reset(void) { return 0; }

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
	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) { filp_close(file, NULL); return -ENOMEM; }
	len = kernel_read(file, buf, PAGE_SIZE - 1, &pos);
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
	.func_set = ki_module_func_set,
	.func_unset = ki_module_func_unset,
	.func_reset = ki_module_func_reset,
	.get_real = ki_module_get_real,
	.get_real_key = ki_module_get_real_key,
};
