/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_H
#define _KI_H

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/string.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0)
static inline ssize_t ki_compat_strscpy(char *dst, const char *src, size_t size)
{
	size_t len;

	if (!size)
		return -E2BIG;
	len = strlen(src);
	if (len >= size) {
		memcpy(dst, src, size - 1);
		dst[size - 1] = '\0';
		return -E2BIG;
	}
	memcpy(dst, src, len + 1);
	return (ssize_t)len;
}
#define strscpy ki_compat_strscpy
#endif


#include "ki_uapi.h"

struct ki_state {
	bool safemode;
	bool config_active;
	struct mutex lock;
};

extern struct ki_state ki_state;
extern bool ki_debug;

int ki_safemode_init(void);
void ki_safemode_exit(void);
bool ki_is_safemode(void);

int ki_config_active(void);
int ki_config_inactive(void);
bool ki_config_is_active(void);

int ki_config_set(const char *kfunc, const char *key, const char *value);
int ki_config_unset(const char *kfunc, const char *key);
int ki_config_del(const char *kfunc);
int ki_config_reset(const char *kfunc);
int ki_config_reload(void);

int ki_func_set(const char *kfunc, const char *key, const char *value);
int ki_func_unset(const char *kfunc, const char *key);
int ki_func_reset(const char *kfunc);

int ki_get_real(const char *kfunc, const char *key,
	       char *value, size_t size, char *out_kfunc, size_t out_kfunc_size);

int ki_device_init(void);
void ki_device_exit(void);

int ki_hook_init(void);
void ki_hook_exit(void);

#endif /* _KI_H */
