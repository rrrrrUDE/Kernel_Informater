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
	KI_IOCTL_NR_GET_DEBUG,
	KI_IOCTL_NR_GET_KFUNC_FEATURES,
	KI_IOCTL_NR_CONFIG_ACTIVE,
	KI_IOCTL_NR_CONFIG_INACTIVE,
	KI_IOCTL_NR_FUNC_SET,
	KI_IOCTL_NR_FUNC_UNSET,
	KI_IOCTL_NR_FUNC_RESET,
	KI_IOCTL_NR_GET_REAL,
	KI_IOCTL_NR_GET_KFUNC_LIST,
};

struct ki_ioc_version {
	__u32 major;
	__u32 minor;
	__u32 patch;
};

struct ki_ioc_debug {
	__u32 enabled;
};

#define KI_KFUNC_FEATURE_CONFIG   (1U << 0)
#define KI_KFUNC_FEATURE_FUNC     (1U << 1)
#define KI_KFUNC_FEATURE_GET_REAL (1U << 2)

struct ki_ioc_kfunc_features {
	char kfunc[KI_UAPI_KFUNC_MAX];
	__u32 features;
};

struct ki_ioc_kfunc_info {
	__u32 index;
	__u32 features;
	char kfunc[KI_UAPI_KFUNC_MAX];
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

#define KI_IOC_GET_VERSION 	_IOR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_VERSION, struct ki_ioc_version)
#define KI_IOC_GET_DEBUG 	_IOR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_DEBUG, struct ki_ioc_debug)
#define KI_IOC_GET_KFUNC_FEATURES 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_KFUNC_FEATURES, struct ki_ioc_kfunc_features)
#define KI_IOC_GET_KFUNC_LIST 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_KFUNC_LIST, struct ki_ioc_kfunc_info)
#define KI_IOC_CONFIG_ON 	_IO(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_ACTIVE)
#define KI_IOC_CONFIG_OFF 	_IO(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_INACTIVE)
#define KI_IOC_FUNC_VALUE_SET 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_SET, struct ki_ioc_value)
#define KI_IOC_FUNC_VALUE_UNSET 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_UNSET, struct ki_ioc_key)
#define KI_IOC_FUNC_KFUNC_RESET 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_RESET, struct ki_ioc_kfunc)
#define KI_IOC_GET_REAL_INFO 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_REAL, struct ki_ioc_real)

#endif /* _KI_UAPI_H */
