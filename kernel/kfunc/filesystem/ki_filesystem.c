/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/namei.h>
#include <linux/cred.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
#include <linux/statfs.h>
#endif
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/user_namespace.h>
#include <linux/rcupdate.h>
#include <linux/kallsyms.h>
#include <linux/mount.h>
#include <linux/uaccess.h>

#include "ki.h"
#include "ki_fs_compat.h"
#include "ki_kfunc.h"
#include "ki_filesystem.h"

#define KI_FS_PATH_MAX 256
#define KI_FS_CONFIG_MAX 16
#define KI_FS_LIST_MAX (64 * 1024)

/*
 * Filesystem kfunc is intentionally read/inspection oriented.
 *
 * Do not invoke mount(2), umount(2), toybox, or another userspace helper
 * from the kernel here.  This keeps the kfunc usable in both built-in and
 * modular integration environments and avoids a shell/userspace dependency.
 */
struct ki_fs_path_array {
	struct rcu_head rcu;
	char paths[KI_FS_CONFIG_MAX][KI_FS_PATH_MAX];
};

static struct ki_fs_path_array ki_fs_paths_boot;
static struct ki_fs_path_array __rcu *ki_fs_paths = &ki_fs_paths_boot;
static DEFINE_MUTEX(ki_fs_lock);

static int ki_filesystem_config_set(const char *key, const char *value)
{
	struct ki_fs_path_array *old;
	struct ki_fs_path_array *new_paths;
	unsigned int index;

	if (!key || !value || strncmp(key, "path.", 5))
		return -EINVAL;

	if (kstrtouint(key + 5, 10, &index) ||
	    index >= KI_FS_CONFIG_MAX)
		return -EINVAL;

	if (strlen(value) >= KI_FS_PATH_MAX)
		return -ENAMETOOLONG;

	new_paths = kmalloc(sizeof(*new_paths), GFP_KERNEL);
	if (!new_paths)
		return -ENOMEM;

	mutex_lock(&ki_fs_lock);
	old = rcu_dereference_protected(ki_fs_paths,
					lockdep_is_held(&ki_fs_lock));
		memcpy(new_paths->paths, old->paths, sizeof(new_paths->paths));
		ki_fs_strscpy(new_paths->paths[index], value,
				      sizeof(new_paths->paths[index]));
	rcu_assign_pointer(ki_fs_paths, new_paths);
	mutex_unlock(&ki_fs_lock);

	if (old != &ki_fs_paths_boot)
		kfree_rcu(old, rcu);
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
	struct ki_fs_path_array *old;
	struct ki_fs_path_array *new_paths;

	new_paths = kzalloc(sizeof(*new_paths), GFP_KERNEL);
	if (!new_paths)
		return -ENOMEM;

	mutex_lock(&ki_fs_lock);
	old = rcu_dereference_protected(ki_fs_paths,
					lockdep_is_held(&ki_fs_lock));
	rcu_assign_pointer(ki_fs_paths, new_paths);
	mutex_unlock(&ki_fs_lock);

	if (old != &ki_fs_paths_boot)
		kfree_rcu(old, rcu);
	return 0;
}

static int ki_filesystem_stat(const char *path_name, char *value, size_t size)
{
	struct path path;
	struct kstat stat;
	char *copy;
	int ret;

	if (!path_name || !*path_name || !value || !size)
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	copy = kstrdup(path_name, GFP_KERNEL);
	if (!copy)
		return -ENOMEM;

	ret = kern_path(copy, LOOKUP_FOLLOW, &path);
	if (ret)
		goto out_free;

	memset(&stat, 0, sizeof(stat));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	ret = vfs_getattr(&path, &stat, STATX_BASIC_STATS,
			  AT_STATX_SYNC_AS_STAT);
#else
	ret = vfs_getattr(&path, &stat);
#endif
	if (!ret) {
		snprintf(value, size,
			 "mode=%#o size=%lld blocks=%lld ino=%llu nlink=%u "
			 "uid=%u gid=%u dev=%u:%u",
			 stat.mode,
			 (long long)stat.size,
			 (long long)stat.blocks,
			 (unsigned long long)stat.ino,
			 (unsigned int)stat.nlink,
			 from_kuid_munged(current_user_ns(), stat.uid),
			 from_kgid_munged(current_user_ns(), stat.gid),
			 MAJOR(stat.dev), MINOR(stat.dev));
	}

	path_put(&path);
out_free:
	kfree(copy);
	return ret;
}

static int ki_filesystem_func_set(const char *key, const char *value)
{
	/*
	 * Keep runtime filesystem operations non-mutating.  Mount topology
	 * changes belong to the normal mount(2)/umount(2) interface instead of
	 * being implemented through a shell helper inside the kernel.
	 */
	if (!key || !value)
		return -EINVAL;

	return -EOPNOTSUPP;
}

static int ki_filesystem_func_unset(const char *key)
{
	if (key && *key)
		return -EOPNOTSUPP;
	return -EINVAL;
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

	if (!strncmp(key, "stat:", 5))
		return ki_filesystem_stat(key + 5, value, size);

	if (!strcmp(key, "count")) {
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

		rcu_read_lock();
		{
			struct ki_fs_path_array *paths =
				rcu_dereference(ki_fs_paths);
			ki_fs_strscpy(value, paths->paths[i], size);
		}
		rcu_read_unlock();
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
		ki_fs_strscpy(key, "count", size);
		return 0;
	}

	index--;
	if (index >= KI_FS_CONFIG_MAX)
		return -ENOENT;

	snprintf(key, size, "path.%u", index);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
typedef int (*ki_mount_fn_t)(char __user *, char __user *, char __user *,
				unsigned long, void __user *);
typedef int (*ki_umount_fn_t)(char __user *, int);

static ki_mount_fn_t ki_mount_fn;
static ki_umount_fn_t ki_umount_fn;

static int ki_filesystem_mount_resolve(void)
{
	if (ki_mount_fn && ki_umount_fn)
		return 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
	ki_mount_fn = (ki_mount_fn_t)kallsyms_lookup_name("sys_mount");
	ki_umount_fn = (ki_umount_fn_t)kallsyms_lookup_name("sys_umount");
#else
	ki_mount_fn = (ki_mount_fn_t)kallsyms_lookup_name("ksys_mount");
	ki_umount_fn = (ki_umount_fn_t)kallsyms_lookup_name("ksys_umount");
#endif

	if (!ki_mount_fn || !ki_umount_fn)
		return -ENOSYS;
	return 0;
}
#endif

long ki_filesystem_mount(const struct ki_ioc_filesystem_mount *request)
{
	if (!request)
		return -EINVAL;

	if (request->operation == KI_FILESYSTEM_MOUNT_ADD) {
		char source[KI_FS_PATH_MAX];
		char target[KI_FS_PATH_MAX];
		ssize_t source_len;
		ssize_t target_len;

		if (!request->source || !request->target)
			return -EFAULT;
		source_len = strncpy_from_user(source,
			(const char __user *)(unsigned long)request->source,
			sizeof(source));
		if (source_len < 0)
			return source_len;
		if (source_len >= sizeof(source))
			return -ENAMETOOLONG;

		target_len = strncpy_from_user(target,
			(const char __user *)(unsigned long)request->target,
			sizeof(target));
		if (target_len < 0)
			return target_len;
		if (target_len >= sizeof(target))
			return -ENAMETOOLONG;
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
		{
			long ret = ki_filesystem_mount_resolve();
			if (ret)
				return ret;
		}
		return ki_mount_fn((char __user *)(unsigned long)request->source,
				   (char __user *)(unsigned long)request->target,
				   NULL, MS_BIND, NULL);
#else
		return -EOPNOTSUPP;
#endif
	}

	if (request->operation == KI_FILESYSTEM_MOUNT_UMOUNT ||
	    request->operation == KI_FILESYSTEM_MOUNT_HOT_UMOUNT) {
		char target[KI_FS_PATH_MAX];

		if (!request->target)
			return -EFAULT;
		if (strncpy_from_user(target, (const char __user *)(uintptr_t)request->target,
				      sizeof(target)) <= 0)
			return -EFAULT;
		if (strlen(target) >= sizeof(target))
			return -ENAMETOOLONG;
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
{
			long ret = ki_filesystem_mount_resolve();
			int flags = request->flags & MNT_DETACH;
			if (ret)
				return ret;
			return ki_umount_fn((char __user *)(uintptr_t)request->target, flags);
		}
#else
		return -EOPNOTSUPP;
#endif
	}

	return -EINVAL;
}

int ki_filesystem_list_line(unsigned int index, char *line, size_t size)
{
	struct file *file;
	char *buf;
	char *cursor;
	char *next;
	loff_t pos = 0;
	ssize_t len;
	unsigned int line_index = 0;

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

		if (line_index++ == index) {
			ki_fs_strscpy(line, cursor, size);
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
