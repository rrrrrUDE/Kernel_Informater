/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "ki.h"
#include "ki_fs_compat.h"
#include "ki_kfunc.h"

static int ki_config_for_each_reset(void)
{
	return ki_kfunc_reset_all_config();
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
	bool already_active = ki_config_is_active();
	int ret;

	if (already_active)
		return 1;

	ret = ki_config_reload();
	if (ret)
		return ret;

	pr_info("KI: persistent configuration activated\n");
	return 0;
}

int ki_config_inactive(void)
{
	ki_config_for_each_reset();

	mutex_lock(&ki_state.lock);
	ki_state.config_active = false;
	mutex_unlock(&ki_state.lock);

	pr_info("KI: persistent configuration deactivated and reset\n");
	return 0;
}

int ki_config_set(const char *kfunc, const char *key, const char *value)
{
	struct ki_kfunc *func;

	if (!kfunc || !key || !value)
		return -EINVAL;

	func = ki_kfunc_find(kfunc);
	if (!func || !func->config_set || !func->config_unset || !func->config_reset)
		return -EOPNOTSUPP;

	return func->config_set(key, value);
}

int ki_config_unset(const char *kfunc, const char *key)
{
	struct ki_kfunc *func;

	if (!kfunc || !key)
		return -EINVAL;

	func = ki_kfunc_find(kfunc);
	if (!func || !func->config_set || !func->config_unset || !func->config_reset)
		return -EOPNOTSUPP;

	return func->config_unset(key);
}

int ki_config_del(const char *kfunc)
{
	struct ki_kfunc *func;

	if (!kfunc || !*kfunc)
		return -EINVAL;

	func = ki_kfunc_find(kfunc);
	if (!func || !func->config_set || !func->config_unset || !func->config_reset)
		return -EOPNOTSUPP;

	return func->config_reset();
}

int ki_config_reset(const char *kfunc)
{
	if (kfunc && *kfunc)
		return ki_config_del(kfunc);

	return ki_config_for_each_reset();
}

#define KI_CONFIG_PATH "/data/adb/ki_user/config"
#define KI_CONFIG_MAX_SIZE (64 * 1024)

int ki_config_reload(void)
{
	struct file *file;
	char *buf;
	loff_t pos = 0;
	ssize_t len;
	char *line;
	char *cursor;
	int ret = 0;
	bool active = false;

	file = filp_open(KI_CONFIG_PATH, O_RDONLY | O_CLOEXEC, 0);
	if (IS_ERR(file)) {
		if (PTR_ERR(file) == -ENOENT) {
			mutex_lock(&ki_state.lock);
			ki_state.config_active = false;
			mutex_unlock(&ki_state.lock);
			ki_config_for_each_reset();
			return 0;
		}
		return PTR_ERR(file);
	}

	buf = kzalloc(KI_CONFIG_MAX_SIZE + 1, GFP_KERNEL);
	if (!buf) {
		filp_close(file, NULL);
		return -ENOMEM;
	}

	len = ki_kernel_read(file, buf, KI_CONFIG_MAX_SIZE, &pos);
	filp_close(file, NULL);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	/* Ignore an over-sized configuration rather than parsing a truncated line. */
	if (len == KI_CONFIG_MAX_SIZE) {
		kfree(buf);
		return -EFBIG;
	}

	ki_config_for_each_reset();

	cursor = buf;
	while ((line = strsep(&cursor, "\n")) != NULL) {
		char *eq;
		char *key;
		char *value;

		line = strim(line);
		if (!*line || *line == '#')
			continue;
		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = '\0';
		key = strim(line);
		value = strim(eq + 1);

		if (!strcmp(key, "active")) {
			if (!strcmp(value, "1"))
				active = true;
			else if (!strcmp(value, "0"))
				active = false;
		}
	}

	/* inactive means no persistent overrides may remain applied. */
	if (!active) {
		ki_config_for_each_reset();
	} else {
		cursor = buf;
		while ((line = strsep(&cursor, "\n")) != NULL) {
			char *eq;
			char *key;
			char *value;
			char *dot;

			line = strim(line);
			if (!*line || *line == '#')
				continue;
			eq = strchr(line, '=');
			if (!eq)
				continue;
			*eq = '\0';
			key = strim(line);
			value = strim(eq + 1);
			if (!strcmp(key, "active"))
				continue;

			dot = strchr(key, '.');
			if (!dot || dot == key || !*(dot + 1))
				continue;
			*dot = '\0';
			ret = ki_config_set(key, dot + 1, value);
			if (ret) {
				pr_warn("KI: invalid config %s=%s: %d\n",
					key, value, ret);
				break;
			}
		}
	}

	if (ret)
		ki_config_for_each_reset();

	mutex_lock(&ki_state.lock);
	ki_state.config_active = active && !ret;
	mutex_unlock(&ki_state.lock);

	kfree(buf);
	pr_info("KI: persistent configuration reloaded\n");
	return 0;
}
