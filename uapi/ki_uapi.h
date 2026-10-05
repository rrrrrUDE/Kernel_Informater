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
#define KI_UAPI_PROCESS_COMM_MAX 64
#define KI_UAPI_PROCESS_READ_MAX 4096

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
	KI_IOCTL_NR_CONFIG_SYNC,
	KI_IOCTL_NR_GET_REAL_KEY_LIST,
	KI_IOCTL_NR_PROCESS_LIST,
	KI_IOCTL_NR_PROCESS_INFO,
	KI_IOCTL_NR_PROCESS_READ_MEMORY,
	KI_IOCTL_NR_PROCESS_KILL,
	KI_IOCTL_NR_PROCESS_KILL_TREE,
	KI_IOCTL_NR_LIST_LINE,
	KI_IOCTL_NR_FILESYSTEM_MOUNT,
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

struct ki_ioc_real_key_info {
	__u32 index;
	__u32 reserved;
	char kfunc[KI_UAPI_KFUNC_MAX];
	char key[KI_UAPI_KEY_MAX];
};

struct ki_ioc_process_entry {
	__u32 index;
	__s32 pid;
	__s32 ppid;
	__u32 uid;
	__u32 state;
	char comm[KI_UAPI_PROCESS_COMM_MAX];
};

struct ki_ioc_process_info {
	__s32 pid;
	__s32 ppid;
	__s32 tgid;
	__u32 uid;
	__u32 gid;
	__u32 state;
	__u32 flags;
	__u64 start_time;
	__u64 virtual_size;
	__u64 resident_pages;
	char comm[KI_UAPI_PROCESS_COMM_MAX];
};

struct ki_ioc_process_read {
	__s32 pid;
	__u32 size;
	__u64 address;
	__u8 data[KI_UAPI_PROCESS_READ_MAX];
};

struct ki_ioc_process_pid {
	__s32 pid;
};

#define KI_LIST_MODULE 1U
#define KI_LIST_FILESYSTEM 2U
#define KI_LIST_MOUNT KI_LIST_FILESYSTEM
#define KI_UAPI_LIST_LINE_MAX 1024

struct ki_ioc_list_line {
	__u32 type;
	__u32 index;
	char line[KI_UAPI_LIST_LINE_MAX];
};

#define KI_FILESYSTEM_MOUNT_ADD        1U
#define KI_FILESYSTEM_MOUNT_UMOUNT     2U
#define KI_FILESYSTEM_MOUNT_HOT_UMOUNT 3U

struct ki_ioc_filesystem_mount {
	__u32 operation;
	__u32 flags;
	__u64 source;
	__u64 target;
};

#define KI_IOC_GET_VERSION 	_IOR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_VERSION, struct ki_ioc_version)
#define KI_IOC_GET_DEBUG 	_IOR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_DEBUG, struct ki_ioc_debug)
#define KI_IOC_GET_KFUNC_FEATURES 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_KFUNC_FEATURES, struct ki_ioc_kfunc_features)
#define KI_IOC_GET_KFUNC_LIST 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_KFUNC_LIST, struct ki_ioc_kfunc_info)
#define KI_IOC_CONFIG_ON 	_IO(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_ACTIVE)
#define KI_IOC_CONFIG_OFF 	_IO(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_INACTIVE)
#define KI_IOC_CONFIG_SYNC 	_IO(KI_IOC_MAGIC, KI_IOCTL_NR_CONFIG_SYNC)
#define KI_IOC_GET_REAL_KEY_LIST 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_REAL_KEY_LIST, struct ki_ioc_real_key_info)
#define KI_IOC_FUNC_VALUE_SET 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_SET, struct ki_ioc_value)
#define KI_IOC_FUNC_VALUE_UNSET 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_UNSET, struct ki_ioc_key)
#define KI_IOC_FUNC_KFUNC_RESET 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FUNC_RESET, struct ki_ioc_kfunc)
#define KI_IOC_GET_REAL_INFO 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_GET_REAL, struct ki_ioc_real)
#define KI_IOC_PROCESS_LIST 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_PROCESS_LIST, struct ki_ioc_process_entry)
#define KI_IOC_PROCESS_INFO 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_PROCESS_INFO, struct ki_ioc_process_info)
#define KI_IOC_PROCESS_READ_MEMORY 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_PROCESS_READ_MEMORY, struct ki_ioc_process_read)
#define KI_IOC_PROCESS_KILL 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_PROCESS_KILL, struct ki_ioc_process_pid)
#define KI_IOC_PROCESS_KILL_TREE 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_PROCESS_KILL_TREE, struct ki_ioc_process_pid)
#define KI_IOC_LIST_LINE 	_IOWR(KI_IOC_MAGIC, KI_IOCTL_NR_LIST_LINE, struct ki_ioc_list_line)
#define KI_IOC_FILESYSTEM_MOUNT 	_IOW(KI_IOC_MAGIC, KI_IOCTL_NR_FILESYSTEM_MOUNT, struct ki_ioc_filesystem_mount)

#endif /* _KI_UAPI_H */
