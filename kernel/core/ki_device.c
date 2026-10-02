/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "ki.h"
#include "ki_kfunc.h"

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

static long ki_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	(void)file;

	switch (cmd) {
	case KI_IOC_GET_VERSION: {
		struct ki_ioc_version version = {
			.major = KI_VERSION_MAJOR,
			.minor = KI_VERSION_MINOR,
			.patch = KI_VERSION_PATCH,
		};
		if (copy_to_user((void __user *)arg, &version, sizeof(version)))
			return -EFAULT;
		return 0;
	}
	case KI_IOC_GET_STATUS: {
		struct ki_ioc_status status = {
			.safemode = ki_is_safemode(),
			.config_active = ki_config_is_active(),
		};
		if (copy_to_user((void __user *)arg, &status, sizeof(status)))
			return -EFAULT;
		return 0;
	}
	case KI_IOC_SAFE_MODE:
		return -EOPNOTSUPP;
	case KI_IOC_CONFIG_RELOAD:
		return ki_config_reload();
	case KI_IOC_CONFIG_VALUE_SET:
		return -EOPNOTSUPP;
	case KI_IOC_CONFIG_KFUNC_DEL:
		return -EOPNOTSUPP;
	case KI_IOC_CONFIG_KFUNC_RESET:
		return -EOPNOTSUPP;
	case KI_IOC_CONFIG_ON:
	case KI_IOC_CONFIG_OFF:
		return -EOPNOTSUPP;
	case KI_IOC_FUNC_VALUE_SET: {
		struct ki_ioc_value value;
		int ret = ki_copy_ioc_value(&value, arg);
		if (ret)
			return ret;
		return ki_func_set(value.kfunc, value.key, value.value);
	}
	case KI_IOC_FUNC_VALUE_UNSET: {
		struct ki_ioc_key key;
		int ret = ki_copy_ioc_key(&key, arg);
		if (ret)
			return ret;
		return ki_func_unset(key.kfunc, key.key);
	}
	case KI_IOC_FUNC_KFUNC_RESET: {
		struct ki_ioc_kfunc kfunc;
		int ret = ki_copy_ioc_kfunc(&kfunc, arg);
		if (ret)
			return ret;
		return ki_func_reset(kfunc.kfunc);
	}
	case KI_IOC_GET_REAL_INFO: {
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
	default:
		return -ENOTTY;
	}
}

static const struct file_operations ki_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ki_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ki_ioctl,
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
