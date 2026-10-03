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
#include <linux/user_namespace.h>

#include "ki.h"
#include "ki_fs_compat.h"
#include "ki_kfunc.h"
#include "ki_filesystem.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
extern int path_umount(struct path *path, int flags);
#else
extern int ksys_umount(char __user *name, int flags);
#endif

#define KI_FS_PATH_MAX 256
#define KI_FS_CONFIG_MAX 16
#define KI_FS_LIST_MAX (64 * 1024)

static char ki_fs_paths[KI_FS_CONFIG_MAX][KI_FS_PATH_MAX];
static DEFINE_MUTEX(ki_fs_lock);

static int ki_filesystem_run(char *const argv[])
{
	char *envp[] = {
		"HOME=/",
		"PATH=/system/bin:/system/xbin:/vendor/bin",
		NULL
	};

	return call_usermodehelper("/system/bin/toybox", argv, envp,
				   UMH_WAIT_PROC);
}

static int ki_filesystem_bind(const char *source, const char *target)
{
	char *argv[] = {
		"mount", "--bind", (char *)source, (char *)target, NULL
	};

	if (!source || !*source || !target || !*target)
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	return ki_filesystem_run(argv);
}

static int ki_filesystem_umount(const char *target, bool lazy)
{
	if (!target || !*target)
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	{
		struct path path;
		int ret = kern_path(target, LOOKUP_FOLLOW, &path);

		if (ret)
			return ret;
		ret = path_umount(&path, lazy ? MNT_DETACH : 0);
		path_put(&path);
		return ret;
	}
#else
	{
		mm_segment_t old_fs = get_fs();
		int ret;

		set_fs(KERNEL_DS);
		ret = ksys_umount((char __user *)target,
				  lazy ? MNT_DETACH : 0);
		set_fs(old_fs);
		return ret;
	}
#endif
}

static int ki_filesystem_config_set(const char *key, const char *value)
{
	unsigned int index;

	if (!key || !value || strncmp(key, "path.", 5))
		return -EINVAL;

	if (kstrtouint(key + 5, 10, &index) ||
	    index >= KI_FS_CONFIG_MAX)
		return -EINVAL;

	if (strlen(value) >= KI_FS_PATH_MAX)
		return -ENAMETOOLONG;

	mutex_lock(&ki_fs_lock);
	strscpy(ki_fs_paths[index], value, sizeof(ki_fs_paths[index]));
	mutex_unlock(&ki_fs_lock);
	return 0;
}

static int ki_filesystem_config_unset(const char *key)
{
	if (!key || strncmp(key, "path.", 5))
		return -EINVAL;
	return ki_filesystem_config_set(key, "");
}

static int ki_filesystem_config_reset(void)
{
	unsigned int i;

	mutex_lock(&ki_fs_lock);
	for (i = 0; i < KI_FS_CONFIG_MAX; i++)
		ki_fs_paths[i][0] = '\0';
	mutex_unlock(&ki_fs_lock);
	return 0;
}

static int ki_filesystem_stat(const char *path_name, char *value, size_t size)
{
	struct path path;
	struct kstat stat;
	kuid_t uid;
	kgid_t gid;
	int ret;

	if (!path_name || !*path_name || !value || !size)
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	ret = kern_path(path_name, LOOKUP_FOLLOW, &path);
	if (ret)
		return ret;

	memset(&stat, 0, sizeof(stat));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	ret = vfs_getattr(&path, &stat, STATX_BASIC_STATS,
			  AT_STATX_SYNC_AS_STAT);
#else
	ret = vfs_getattr(&path, &stat);
#endif
	if (ret)
		goto out;

	uid = stat.uid;
	gid = stat.gid;
	snprintf(value, size,
		 "mode=%#o size=%lld blocks=%lld ino=%llu nlink=%u "
		 "uid=%u gid=%u dev=%u:%u",
		 stat.mode,
		 (long long)stat.size,
		 (long long)stat.blocks,
		 (unsigned long long)stat.ino,
		 (unsigned int)stat.nlink,
		 from_kuid_munged(current_user_ns(), uid),
		 from_kgid_munged(current_user_ns(), gid),
		 MAJOR(stat.dev), MINOR(stat.dev));
out:
	path_put(&path);
	return ret;
}

static int ki_filesystem_func_set(const char *key, const char *value)
{
	char *original;
	char *source;
	char *target;
	int ret;

	if (!key || !value || !*value)
		return -EINVAL;

	if (!strcmp(key, "stat"))
		return ki_filesystem_stat(value, (char *)value, KI_FS_PATH_MAX);

	if (!strcmp(key, "umount"))
		return ki_filesystem_umount(value, false);

	if (!strcmp(key, "hot_unmount"))
		return ki_filesystem_umount(value, true);

	if (strcmp(key, "add"))
		return -EINVAL;

	original = kstrdup(value, GFP_KERNEL);
	if (!original)
		return -ENOMEM;

	target = original;
	source = strsep(&target, "\t");
	if (!source || !target || !*source || !*target)
		ret = -EINVAL;
	else
		ret = ki_filesystem_bind(source, target);

	kfree(original);
	return ret;
}

static int ki_filesystem_func_unset(const char *key)
{
	return key && *key ? -EOPNOTSUPP : -EINVAL;
}

static int ki_filesystem_func_reset(void)
{
	return 0;
}

static int ki_filesystem_get_real(const char *key, char *value, size_t size)
{
	struct file *file;
	char *buf;
	loff_t pos = 0;
	ssize_t len;
	unsigned int count = 0;
	unsigned int i;

	if (!key || !value || !size)
		return -EINVAL;

	if (!strcmp(key, "count")) {
		file = filp_open("/proc/self/mountinfo",
				 O_RDONLY | O_CLOEXEC, 0);
		if (IS_ERR(file))
			return PTR_ERR(file);

		buf = kzalloc(KI_FS_LIST_MAX, GFP_KERNEL);
		if (!buf) {
			filp_close(file, NULL);
			return -ENOMEM;
		}

		len = ki_kernel_read(file, buf, KI_FS_LIST_MAX - 1, &pos);
		filp_close(file, NULL);
		if (len < 0) {
			kfree(buf);
			return len;
		}

		while (len-- > 0)
			if (buf[len] == '\n')
				count++;

		snprintf(value, size, "%u", count);
		kfree(buf);
		return 0;
	}

	if (!strncmp(key, "path.", 5)) {
		if (kstrtouint(key + 5, 10, &i) ||
		    i >= KI_FS_CONFIG_MAX)
			return -EINVAL;

		mutex_lock(&ki_fs_lock);
		strscpy(value, ki_fs_paths[i], size);
		mutex_unlock(&ki_fs_lock);
		return 0;
	}

	return -EINVAL;
}

static int ki_filesystem_get_real_key(unsigned int index,
				      char *key, size_t size)
{
	if (!key || !size)
		return -EINVAL;

	if (index == 0) {
		strscpy(key, "count", size);
		return 0;
	}

	index--;
	if (index >= KI_FS_CONFIG_MAX)
		return -ENOENT;

	snprintf(key, size, "path.%u", index);
	return 0;
}

int ki_filesystem_list_line(unsigned int index, char *line, size_t size)
{
	struct file *file;
	char *buf;
	char *cursor;
	char *next;
	loff_t pos = 0;
	ssize_t len;
	unsigned int current = 0;

	if (!line || !size)
		return -EINVAL;

	file = filp_open("/proc/self/mountinfo", O_RDONLY | O_CLOEXEC, 0);
	if (IS_ERR(file))
		return PTR_ERR(file);

	buf = kzalloc(KI_FS_LIST_MAX, GFP_KERNEL);
	if (!buf) {
		filp_close(file, NULL);
		return -ENOMEM;
	}

	len = ki_kernel_read(file, buf, KI_FS_LIST_MAX - 1, &pos);
	filp_close(file, NULL);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';
	cursor = buf;

	while (cursor && *cursor) {
		next = strchr(cursor, '\n');
		if (next)
			*next++ = '\0';

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

struct ki_kfunc ki_filesystem_kfunc = {
	.name = "filesystem",
	.config_set = ki_filesystem_config_set,
	.config_unset = ki_filesystem_config_unset,
	.config_reset = ki_filesystem_config_reset,
	.func_set = ki_filesystem_func_set,
	.func_unset = ki_filesystem_func_unset,
	.func_reset = ki_filesystem_func_reset,
	.get_real = ki_filesystem_get_real,
	.get_real_key = ki_filesystem_get_real_key,
};
