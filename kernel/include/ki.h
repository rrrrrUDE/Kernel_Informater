/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_H
#define _KI_H

#include <linux/mutex.h>
#include <linux/types.h>

#include "ki_uapi.h"

struct ki_state {
	bool safemode;
	bool config_active;
	struct mutex lock;
};

extern struct ki_state ki_state;

int ki_set_safemode(bool enable);
bool ki_is_safemode(void);

int ki_config_active(void);
int ki_config_inactive(void);
bool ki_config_is_active(void);

int ki_config_set(const char *kfunc, const char *key, const char *value);
int ki_config_unset(const char *kfunc, const char *key);
int ki_config_del(const char *kfunc);
int ki_config_reset(const char *kfunc);

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
