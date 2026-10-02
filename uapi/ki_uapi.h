/* SPDX-License-Identifier: GPL-2.0 OR MIT */
#ifndef _KI_UAPI_H
#define _KI_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define KI_VERSION_MAJOR 1
#define KI_VERSION_MINOR 0
#define KI_VERSION_PATCH 0
#define KI_VERSION_STRING "1.0.0"

#define KI_DEVICE_NAME "ki"
#define KI_DEVICE_PATH "/dev/ki"

#define KI_USER_DIR "/data/adb/ki_user"
#define KI_USER_CONFIG_PATH KI_USER_DIR "/config"
#define KI_USER_SAFE_MODE_PATH KI_USER_DIR "/safemode"
#define KI_USER_DEBUG_LOG_PATH KI_USER_DIR "/debug.log"

#define KI_UAPI_KFUNC_MAX 64
#define KI_UAPI_KEY_MAX 64
#define KI_UAPI_VALUE_MAX 256

#define KI_IOC_MAGIC 'K'

enum ki_ioctl_nr {
	KI_IOCTL_NR_GET_VERSION = 0,
	KI_IOCTL_NR_GET_STATUS,
	KI_IOCTL_NR_SAFE_MODE,
	KI_IOCTL_NR_CONFIG_SET,
	KI_IOCTL_NR_CONFIG_UNSET,
	KI_IOCTL_NR_CONFIG_DEL,
	KI_IOCTL_NR_CONFIG_RESET,
	KI_IOCTL_NR_CONFIG_ACTIVE,
	KI_IOCTL_NR_CONFIG_INACTIVE,
	KI_IOCTL_NR_FUNC_SET,
	KI_IOCTL_NR_FUNC_UNSET,
	KI_IOCTL_NR_FUNC_RESET,
	KI_IOCTL_NR_GET_REAL,
	KI_IOCTL_NR_CONFIG_RELOAD,
};

struct ki_ioc_version {
	__u32 major;
	__u32 minor;
	__u32 patch;
};

struct ki_ioc_status {
	__u32 safemode;
	__u32 config_active;
};

struct ki_ioc_value {
	char kfunc[KI_UAPI_KFUNC_MAX];
	char key[KI_UAPI_KEY_MAX];
	char value[KI_UAPI_VALUE_MAX];
};

struct ki_ioc_key {
	char kfunc[KI_UAPI_KFUNC_MAX];
	char key[KI_UAPI_KEY_MAX];
};

struct ki_ioc_kfunc {
	char kfunc[KI_UAPI_KFUNC_MAX];
};

struct ki_ioc_real {
	char kfunc[KI_UAPI_KFUNC_MAX];
	char key[KI_UAPI_KEY_MAX];
	char value[KI_UAPI_VALUE_MAX];
};

#define KI_IOC_GET_VERSION \
	_IOR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_VERSION, struct ki_ioc_version)
#define KI_IOC_GET_STATUS \
	_IOR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_STATUS, struct ki_ioc_status)
#define KI_IOC_SAFE_MODE \
	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_SAFE_MODE, __u32)
#define KI_IOC_CONFIG_VALUE_SET \
	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_SET, struct ki_ioc_value)
#define KI_IOC_CONFIG_VALUE_UNSET \
	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_UNSET, struct ki_ioc_key)
#define KI_IOC_CONFIG_KFUNC_DEL \
	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_DEL, struct ki_ioc_kfunc)
#define KI_IOC_CONFIG_KFUNC_RESET \
	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_RESET, struct ki_ioc_kfunc)
#define KI_IOC_CONFIG_ON \
	_IO(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_ACTIVE)
#define KI_IOC_CONFIG_OFF \
	_IO(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_INACTIVE)
#define KI_IOC_FUNC_VALUE_SET \
	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_SET, struct ki_ioc_value)
#define KI_IOC_FUNC_VALUE_UNSET \
	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_UNSET, struct ki_ioc_key)
#define KI_IOC_FUNC_KFUNC_RESET \
	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_RESET, struct ki_ioc_kfunc)
#define KI_IOC_GET_REAL_INFO \
	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_REAL, struct ki_ioc_real)
#define KI_IOC_CONFIG_RELOAD \
	_IO(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_RELOAD)

#endif /* _KI_UAPI_H */
