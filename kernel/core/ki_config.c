/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>

#include "ki.h"
#include "ki_kfunc.h"

static int ki_config_for_each_reset(void)
{
	struct ki_kfunc *func;

	func = ki_kfunc_find("uname");
	if (func && func->config_reset)
		return func->config_reset();

	return 0;
}

bool ki_config_is_active(void)
{
	bool value;

	mutex_lock(&ki_state.lock);
	value = ki_state.config_active;
	mutex_unlock(&ki_state.lock);

	return value;
}

int ki_config_active(void)
{
	mutex_lock(&ki_state.lock);
	ki_state.config_active = true;
	mutex_unlock(&ki_state.lock);
	pr_info("KI: persistent configuration activated\n");
	return 0;
}

int ki_config_inactive(void)
{
	mutex_lock(&ki_state.lock);
	ki_state.config_active = false;
	mutex_unlock(&ki_state.lock);
	pr_info("KI: persistent configuration deactivated\n");
	return 0;
}

int ki_config_set(const char *kfunc, const char *key, const char *value)
{
	struct ki_kfunc *func;

	if (!kfunc || !key || !value)
		return -EINVAL;

	func = ki_kfunc_find(kfunc);
	if (!func || !func->config_set)
		return -ENOENT;

	return func->config_set(key, value);
}

int ki_config_unset(const char *kfunc, const char *key)
{
	struct ki_kfunc *func;

	if (!kfunc || !key)
		return -EINVAL;

	func = ki_kfunc_find(kfunc);
	if (!func || !func->config_unset)
		return -ENOENT;

	return func->config_unset(key);
}

int ki_config_del(const char *kfunc)
{
	struct ki_kfunc *func;

	if (!kfunc || !*kfunc)
		return -EINVAL;

	func = ki_kfunc_find(kfunc);
	if (!func || !func->config_reset)
		return -ENOENT;

	return func->config_reset();
}

int ki_config_reset(const char *kfunc)
{
	if (kfunc && *kfunc)
		return ki_config_del(kfunc);

	return ki_config_for_each_reset();
}
