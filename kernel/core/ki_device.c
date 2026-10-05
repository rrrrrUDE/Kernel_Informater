/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "ki.h"
#include "ki_kfunc.h"
#include "ki_process.h"
#include "ki_module.h"
#include "ki_filesystem.h"

static int ki_copy_ioc_value(struct ki_ioc_value *dst, unsigned long arg)
{
	if (copy_from_user(dst, (void __user *)arg, sizeof(*dst)))
		return -EFAULT;
	if (!memchr(dst->kfunc, '\0', sizeof(dst->kfunc)) ||
	    !memchr(dst->key, '\0', sizeof(dst->key)) ||
	    !memchr(dst->value, '\0', sizeof(dst->value)))
		return -EINVAL;
	return 0;
}

static int ki_copy_ioc_key(struct ki_ioc_key *dst, unsigned long arg)
{
	if (copy_from_user(dst, (void __user *)arg, sizeof(*dst)))
		return -EFAULT;
	if (!memchr(dst->kfunc, '\0', sizeof(dst->kfunc)) ||
	    !memchr(dst->key, '\0', sizeof(dst->key)))
		return -EINVAL;
	return 0;
}

static int ki_copy_ioc_kfunc(struct ki_ioc_kfunc *dst, unsigned long arg)
{
	if (copy_from_user(dst, (void __user *)arg, sizeof(*dst)))
		return -EFAULT;
	if (!memchr(dst->kfunc, '\0', sizeof(dst->kfunc)))
		return -EINVAL;
	return 0;
}

static long ki_ioctl_dispatch(unsigned int nr, unsigned long arg)
{
	switch (nr) {
	case KI_IOCTL_NR_GET_VERSION: {
		struct ki_ioc_version version = {
			.major = KI_VERSION_MAJOR,
			.minor = KI_VERSION_MINOR,
			.patch = KI_VERSION_PATCH,
		};
		if (copy_to_user((void __user *)arg, &version, sizeof(version)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_GET_KFUNC_FEATURES: {
		struct ki_ioc_kfunc_features info;
		struct ki_kfunc *kfunc;

		if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
			return -EFAULT;
		if (!memchr(info.kfunc, '\0', sizeof(info.kfunc)))
			return -EINVAL;

		kfunc = ki_kfunc_find(info.kfunc);
		if (!kfunc)
			return -ENOENT;

		info.features = 0;
		if (kfunc->config_set && kfunc->config_unset &&
		    kfunc->config_reset)
			info.features |= KI_KFUNC_FEATURE_CONFIG;
		if (kfunc->func_set && kfunc->func_unset &&
		    kfunc->func_reset)
			info.features |= KI_KFUNC_FEATURE_FUNC;
		if (kfunc->get_real && kfunc->get_real_key)
			info.features |= KI_KFUNC_FEATURE_GET_REAL;

		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_GET_KFUNC_LIST: {
		struct ki_ioc_kfunc_info info;
		struct ki_kfunc *kfunc;
		unsigned int index;

		if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
			return -EFAULT;
		index = info.index;

		kfunc = ki_kfunc_find_by_index(index);
		if (!kfunc)
			return -ENOENT;

		memset(&info, 0, sizeof(info));
		info.index = index;
		strscpy(info.kfunc, kfunc->name, sizeof(info.kfunc));
		if (kfunc->config_set && kfunc->config_unset &&
		    kfunc->config_reset)
			info.features |= KI_KFUNC_FEATURE_CONFIG;
		if (kfunc->func_set && kfunc->func_unset &&
		    kfunc->func_reset)
			info.features |= KI_KFUNC_FEATURE_FUNC;
		if (kfunc->get_real && kfunc->get_real_key)
			info.features |= KI_KFUNC_FEATURE_GET_REAL;

		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_GET_REAL_KEY_LIST: {
		struct ki_ioc_real_key_info info;
		struct ki_kfunc *kfunc;
		char key[KI_UAPI_KEY_MAX];
		unsigned int index;

		if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
			return -EFAULT;
		if (!memchr(info.kfunc, '\0', sizeof(info.kfunc)))
			return -EINVAL;

		index = info.index;
		kfunc = ki_kfunc_find(info.kfunc);
		if (!kfunc || !kfunc->get_real || !kfunc->get_real_key)
			return -EOPNOTSUPP;

		memset(key, 0, sizeof(key));
		if (kfunc->get_real_key(index, key, sizeof(key)))
			return -ENOENT;

		memset(&info, 0, sizeof(info));
		info.index = index;
		strscpy(info.kfunc, kfunc->name, sizeof(info.kfunc));
		strscpy(info.key, key, sizeof(info.key));
		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_GET_DEBUG: {
		struct ki_ioc_debug debug = {
			.enabled = ki_debug ? 1 : 0,
		};
		if (copy_to_user((void __user *)arg, &debug, sizeof(debug)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_CONFIG_ACTIVE:
		return ki_config_active();
	case KI_IOCTL_NR_CONFIG_INACTIVE:
		return ki_config_inactive();
	case KI_IOCTL_NR_CONFIG_SYNC:
		return ki_config_reload();
	case KI_IOCTL_NR_FUNC_SET: {
		struct ki_ioc_value value;
		int ret = ki_copy_ioc_value(&value, arg);
		if (ret)
			return ret;
		return ki_func_set(value.kfunc, value.key, value.value);
	}
	case KI_IOCTL_NR_FUNC_UNSET: {
		struct ki_ioc_key key;
		int ret = ki_copy_ioc_key(&key, arg);
		if (ret)
			return ret;
		return ki_func_unset(key.kfunc, key.key);
	}
	case KI_IOCTL_NR_FUNC_RESET: {
		struct ki_ioc_kfunc kfunc;
		int ret = ki_copy_ioc_kfunc(&kfunc, arg);
		if (ret)
			return ret;
		return ki_func_reset(kfunc.kfunc);
	}
	case KI_IOCTL_NR_GET_REAL: {
		struct ki_ioc_real real;
		int ret;

		if (copy_from_user(&real, (void __user *)arg, sizeof(real)))
			return -EFAULT;
		if (!memchr(real.kfunc, '\0', sizeof(real.kfunc)) ||
		    !memchr(real.key, '\0', sizeof(real.key)))
			return -EINVAL;

		ret = ki_get_real(real.kfunc, real.key,
				  real.value, sizeof(real.value),
				  real.kfunc, sizeof(real.kfunc));
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &real, sizeof(real)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_PROCESS_LIST: {
		struct ki_ioc_process_entry entry;
		int ret;

		if (copy_from_user(&entry, (void __user *)arg, sizeof(entry)))
			return -EFAULT;
		ret = ki_process_list(&entry);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &entry, sizeof(entry)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_PROCESS_INFO: {
		struct ki_ioc_process_info info;
		int ret;

		if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
			return -EFAULT;
		ret = ki_process_info(&info);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_PROCESS_READ_MEMORY: {
		struct ki_ioc_process_read *read;
		int ret;

		read = kmalloc(sizeof(*read), GFP_KERNEL);
		if (!read)
			return -ENOMEM;

		if (copy_from_user(read, (void __user *)arg, sizeof(*read))) {
			ret = -EFAULT;
			goto out_process_read;
		}

		ret = ki_process_read_memory(read);
		if (ret)
			goto out_process_read;

		if (copy_to_user((void __user *)arg, read, sizeof(*read)))
			ret = -EFAULT;

out_process_read:
		kfree(read);
		return ret;
	}
	case KI_IOCTL_NR_LIST_LINE: {
		struct ki_ioc_list_line line;
		int ret;

		if (copy_from_user(&line, (void __user *)arg, sizeof(line)))
			return -EFAULT;
		if (line.type == KI_LIST_MODULE)
			ret = ki_module_list_line(line.index, line.line, sizeof(line.line));
		else if (line.type == KI_LIST_FILESYSTEM)
			ret = ki_filesystem_list_line(line.index, line.line, sizeof(line.line));
		else
			return -EINVAL;
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &line, sizeof(line)))
			return -EFAULT;
		return 0;
	}
	case KI_IOCTL_NR_FILESYSTEM_MOUNT: {
		struct ki_ioc_filesystem_mount request;

		if (copy_from_user(&request, (void __user *)arg, sizeof(request)))
			return -EFAULT;
		if (request.operation < KI_FILESYSTEM_MOUNT_ADD ||
		    request.operation > KI_FILESYSTEM_MOUNT_HOT_UMOUNT)
			return -EINVAL;
		return ki_filesystem_mount(&request);
	}
	case KI_IOCTL_NR_PROCESS_KILL:
	case KI_IOCTL_NR_PROCESS_KILL_TREE: {
		struct ki_ioc_process_pid pid;
		int ret;

		if (copy_from_user(&pid, (void __user *)arg, sizeof(pid)))
			return -EFAULT;
		if (nr == KI_IOCTL_NR_PROCESS_KILL)
			ret = ki_process_kill(pid.pid);
		else
			ret = ki_process_kill_tree(pid.pid);
		return ret;
	}
	default:
		return -ENOTTY;
	}
}

static long ki_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	(void)file;

	/* Keep the external ABI stable while separating entry validation from
	 * command dispatch, following the KSU-style ioctl layering. */
	if (_IOC_TYPE(cmd) != KI_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > KI_IOCTL_NR_FILESYSTEM_MOUNT)
		return -ENOTTY;
	if (_IOC_SIZE(cmd) > PAGE_SIZE)
		return -EINVAL;
	if (_IOC_SIZE(cmd) && !arg)
		return -EFAULT;

	return ki_ioctl_dispatch(_IOC_NR(cmd), arg);
}

#ifdef CONFIG_COMPAT
static long ki_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return ki_ioctl(file, cmd, arg);
}
#endif

static const struct file_operations ki_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ki_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ki_compat_ioctl,
#endif
};

static struct miscdevice ki_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KI_DEVICE_NAME,
	.fops = &ki_fops,
	.mode = 0600,
};

int ki_device_init(void)
{
	return misc_register(&ki_miscdev);
}

void ki_device_exit(void)
{
	misc_deregister(&ki_miscdev);
}
