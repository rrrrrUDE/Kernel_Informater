/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_KFUNC_H
#define _KI_KFUNC_H

#include <linux/list.h>
#include <linux/types.h>

struct ki_kfunc {
	const char *name;

	int (*init)(void);
	void (*exit)(void);

	int (*config_set)(const char *key, const char *value);
	int (*config_unset)(const char *key);
	int (*config_reset)(void);

	int (*func_set)(const char *key, const char *value);
	int (*func_unset)(const char *key);
	int (*func_reset)(void);

	int (*get_real)(const char *key, char *value, size_t size);
};

int ki_kfunc_register(struct ki_kfunc *kfunc);
int ki_kfunc_unregister(struct ki_kfunc *kfunc);
struct ki_kfunc *ki_kfunc_find(const char *name);

#endif /* _KI_KFUNC_H */
