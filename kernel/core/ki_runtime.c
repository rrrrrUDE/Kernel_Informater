/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/kernel.h>

#include "ki.h"
#include "ki_kfunc.h"

bool ki_is_safemode(void)
{
	bool value;

	mutex_lock(&ki_state.lock);
	value = ki_state.safemode;
	mutex_unlock(&ki_state.lock);
	return value;
}

int ki_set_safemode(bool enable)
{
	mutex_lock(&ki_state.lock);
	ki_state.safemode = enable;
	mutex_unlock(&ki_state.lock);
	pr_info("KI: safe mode %s\n", enable ? "enabled" : "disabled");
	return 0;
}

int ki_func_set(const char *kfunc, const char *key, const char *value)
{
	struct ki_kfunc *func;

	if (!kfunc || !key || !value)
		return -EINVAL;
	func = ki_kfunc_find(kfunc);
	if (!func || !func->func_set)
		return -ENOENT;
	return func->func_set(key, value);
}

int ki_func_unset(const char *kfunc, const char *key)
{
	struct ki_kfunc *func;

	if (!kfunc || !key)
		return -EINVAL;
	func = ki_kfunc_find(kfunc);
	if (!func || !func->func_unset)
		return -ENOENT;
	return func->func_unset(key);
}

int ki_func_reset(const char *kfunc)
{
	struct ki_kfunc *func;

	if (kfunc && *kfunc) {
		func = ki_kfunc_find(kfunc);
		if (!func || !func->func_reset)
			return -ENOENT;
		return func->func_reset();
	}

	func = ki_kfunc_find("uname");
	if (func && func->func_reset)
		return func->func_reset();
	return 0;
}

int ki_get_real(const char *kfunc, const char *key,
	       char *value, size_t size,
	       char *out_kfunc, size_t out_kfunc_size)
{
	struct ki_kfunc *func;

	if (!value || !size || !out_kfunc || !out_kfunc_size)
		return -EINVAL;
	if (!kfunc || !*kfunc)
		kfunc = "uname";
	if (!key || !*key)
		key = "release";

	func = ki_kfunc_find(kfunc);
	if (!func || !func->get_real)
		return -ENOENT;

	if (strscpy(out_kfunc, kfunc, out_kfunc_size) < 0)
		return -ENAMETOOLONG;
	return func->get_real(key, value, size);
}
