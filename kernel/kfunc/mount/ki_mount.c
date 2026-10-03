/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kmod.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>
#include "ki.h"
#include "ki_fs_compat.h"
#include "ki_kfunc.h"
#include "ki_mount.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
extern int path_umount(struct path *path, int flags);
#else
extern int ksys_umount(char __user *name, int flags);
#endif

#define KI_MOUNT_PATH_MAX 256
#define KI_MOUNT_CONFIG_MAX 16
#define KI_MOUNT_LIST_MAX (64 * 1024)
static char ki_mount_paths[KI_MOUNT_CONFIG_MAX][KI_MOUNT_PATH_MAX];
static DEFINE_MUTEX(ki_mount_lock);

static int ki_mount_run(char *const argv[])
{
	char *envp[] = {"HOME=/", "PATH=/system/bin:/system/xbin:/vendor/bin", NULL};
	return call_usermodehelper("/system/bin/toybox", argv, envp, UMH_WAIT_PROC);
}

static int ki_mount_bind(const char *source, const char *target)
{
	char *argv[] = {"mount", "--bind", (char *)source, (char *)target, NULL};
	if (!source || !*source || !target || !*target) return -EINVAL;
	if (!capable(CAP_SYS_ADMIN)) return -EPERM;
	return ki_mount_run(argv);
}

static int ki_mount_umount(const char *target, bool lazy)
{
	if (!target || !*target) return -EINVAL;
	if (!capable(CAP_SYS_ADMIN)) return -EPERM;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	{
		struct path path;
		int ret = kern_path(target, LOOKUP_FOLLOW, &path);
		if (ret) return ret;
		return path_umount(&path, lazy ? MNT_DETACH : 0);
	}
#else
	{
		mm_segment_t old_fs = get_fs();
		int ret;
		set_fs(KERNEL_DS);
		ret = ksys_umount((char __user *)target, lazy ? MNT_DETACH : 0);
		set_fs(old_fs);
		return ret;
	}
#endif
}

static int ki_mount_config_set(const char *key, const char *value)
{
	unsigned int index;
	if (!key || !value || strncmp(key, "path.", 5)) return -EINVAL;
	if (kstrtouint(key + 5, 10, &index) || index >= KI_MOUNT_CONFIG_MAX) return -EINVAL;
	if (strlen(value) >= KI_MOUNT_PATH_MAX) return -ENAMETOOLONG;
	mutex_lock(&ki_mount_lock);
	strscpy(ki_mount_paths[index], value, sizeof(ki_mount_paths[index]));
	mutex_unlock(&ki_mount_lock);
	return 0;
}
static int ki_mount_config_unset(const char *key)
{
	if (!key || strncmp(key, "path.", 5)) return -EINVAL;
	return ki_mount_config_set(key, "");
}
static int ki_mount_config_reset(void)
{
	unsigned int i;
	mutex_lock(&ki_mount_lock);
	for (i = 0; i < KI_MOUNT_CONFIG_MAX; i++) ki_mount_paths[i][0] = '\0';
	mutex_unlock(&ki_mount_lock);
	return 0;
}

static int ki_mount_func_set(const char *key, const char *value)
{
	char *original, *source, *target;
	int ret;
	if (!key || !value || !*value) return -EINVAL;
	if (!strcmp(key, "umount")) return ki_mount_umount(value, false);
	if (!strcmp(key, "hot_unmount")) return ki_mount_umount(value, true);
	if (strcmp(key, "add")) return -EINVAL;
	original = kstrdup(value, GFP_KERNEL);
	if (!original) return -ENOMEM;
	target = original;
	source = strsep(&target, "\t");
	if (!source || !target || !*source || !*target) ret = -EINVAL;
	else ret = ki_mount_bind(source, target);
	kfree(original);
	return ret;
}
static int ki_mount_func_unset(const char *key) { return key && *key ? -EOPNOTSUPP : -EINVAL; }
static int ki_mount_func_reset(void) { return 0; }

static int ki_mount_get_real(const char *key, char *value, size_t size)
{
	struct file *file;
	char *buf;
	loff_t pos = 0;
	ssize_t len;
	unsigned int count = 0;
	if (!key || !value || !size || strcmp(key, "count")) return -EINVAL;
	file = filp_open("/proc/self/mountinfo", O_RDONLY | O_CLOEXEC, 0);
	if (IS_ERR(file)) return PTR_ERR(file);
	buf = kzalloc(KI_MOUNT_LIST_MAX, GFP_KERNEL);
	if (!buf) { filp_close(file, NULL); return -ENOMEM; }
	len = ki_kernel_read(file, buf, KI_MOUNT_LIST_MAX - 1, &pos);
	filp_close(file, NULL);
	if (len < 0) { kfree(buf); return len; }
	while (len-- > 0) if (buf[len] == '\n') count++;
	snprintf(value, size, "%u", count);
	kfree(buf);
	return 0;
}
static int ki_mount_get_real_key(unsigned int index, char *key, size_t size)
{
	if (!key || !size || index) return -ENOENT;
	strscpy(key, "count", size);
	return 0;
}

int ki_mount_list_line(unsigned int index, char *line, size_t size)
{
	struct file *file;
	char *buf, *cursor, *next;
	loff_t pos = 0;
	ssize_t len;
	unsigned int current = 0;
	if (!line || !size) return -EINVAL;
	file = filp_open("/proc/self/mountinfo", O_RDONLY | O_CLOEXEC, 0);
	if (IS_ERR(file)) return PTR_ERR(file);
	buf = kzalloc(KI_MOUNT_LIST_MAX, GFP_KERNEL);
	if (!buf) { filp_close(file, NULL); return -ENOMEM; }
	len = ki_kernel_read(file, buf, KI_MOUNT_LIST_MAX - 1, &pos);
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

struct ki_kfunc ki_mount_kfunc = {
	.name = "mount",
	.config_set = ki_mount_config_set,
	.config_unset = ki_mount_config_unset,
	.config_reset = ki_mount_config_reset,
	.func_set = ki_mount_func_set,
	.func_unset = ki_mount_func_unset,
	.func_reset = ki_mount_func_reset,
	.get_real = ki_mount_get_real,
	.get_real_key = ki_mount_get_real_key,
};
