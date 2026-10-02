/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>

#include "ki.h"
#include "ki_uname.h"

#define KI_UNAME_STRING_LEN (__NEW_UTS_LEN + 1)

static DEFINE_MUTEX(ki_uname_lock);

static char ki_uname_config_release[KI_UNAME_STRING_LEN];
static bool ki_uname_config_release_valid;

static char ki_uname_func_release[KI_UNAME_STRING_LEN];
static bool ki_uname_func_release_valid;

static int ki_uname_init(void)
{
	mutex_lock(&ki_uname_lock);
	ki_uname_config_release[0] = '\0';
	ki_uname_config_release_valid = false;
	ki_uname_func_release[0] = '\0';
	ki_uname_func_release_valid = false;
	mutex_unlock(&ki_uname_lock);

	pr_info("KI: kfunc uname initialized\n");
	return 0;
}

static void ki_uname_exit(void)
{
	pr_info("KI: kfunc uname exited\n");
}

static int ki_uname_config_set(const char *key, const char *value)
{
	if (!key || !value || strcmp(key, "release"))
		return -EINVAL;

	mutex_lock(&ki_uname_lock);
	strscpy(ki_uname_config_release, value,
		sizeof(ki_uname_config_release));
	ki_uname_config_release_valid = true;
	mutex_unlock(&ki_uname_lock);

	return 0;
}

static int ki_uname_config_unset(const char *key)
{
	if (!key || strcmp(key, "release"))
		return -EINVAL;

	mutex_lock(&ki_uname_lock);
	ki_uname_config_release[0] = '\0';
	ki_uname_config_release_valid = false;
	mutex_unlock(&ki_uname_lock);

	return 0;
}

static int ki_uname_config_reset(void)
{
	return ki_uname_config_unset("release");
}

static int ki_uname_func_set(const char *key, const char *value)
{
	if (!key || !value || strcmp(key, "release"))
		return -EINVAL;

	mutex_lock(&ki_uname_lock);
	strscpy(ki_uname_func_release, value,
		sizeof(ki_uname_func_release));
	ki_uname_func_release_valid = true;
	mutex_unlock(&ki_uname_lock);

	return 0;
}

static int ki_uname_func_unset(const char *key)
{
	if (!key || strcmp(key, "release"))
		return -EINVAL;

	mutex_lock(&ki_uname_lock);
	ki_uname_func_release[0] = '\0';
	ki_uname_func_release_valid = false;
	mutex_unlock(&ki_uname_lock);

	return 0;
}

static int ki_uname_func_reset(void)
{
	return ki_uname_func_unset("release");
}

static int ki_uname_get_real(const char *key, char *value, size_t size)
{
	const struct new_utsname *u;

	if (!key || !value || !size)
		return -EINVAL;

	/*
	 * uts_sem is not exported on many Android/GKI kernels, which would make
	 * the KI module fail modpost when built independently with M=. The UTS
	 * namespace object itself is stable, so read the selected string directly.
	 * Native uname() still takes uts_sem when constructing its userspace copy.
	 */
	u = utsname();

	if (!strcmp(key, "sysname"))
		strscpy(value, u->sysname, size);
	else if (!strcmp(key, "nodename"))
		strscpy(value, u->nodename, size);
	else if (!strcmp(key, "release"))
		strscpy(value, u->release, size);
	else if (!strcmp(key, "version"))
		strscpy(value, u->version, size);
	else if (!strcmp(key, "machine"))
		strscpy(value, u->machine, size);
	else if (!strcmp(key, "domainname"))
		strscpy(value, u->domainname, size);
	else
		return -EINVAL;

	return 0;
}

static bool ki_uname_get_effective_release(char *value, size_t size)
{
	bool valid = false;

	mutex_lock(&ki_uname_lock);

	if (ki_uname_func_release_valid) {
		strscpy(value, ki_uname_func_release, size);
		valid = true;
	} else if (ki_config_is_active() && ki_uname_config_release_valid) {
		strscpy(value, ki_uname_config_release, size);
		valid = true;
	}

	mutex_unlock(&ki_uname_lock);
	return valid;
}

int ki_uname_override_release(char *release, size_t size)
{
	char value[KI_UNAME_STRING_LEN];

	if (!release || !size)
		return -EINVAL;
	if (ki_is_safemode())
		return 0;

	if (ki_uname_get_effective_release(value, sizeof(value)))
		strscpy(release, value, size);

	return 0;
}

int ki_uname_apply_user_buffer(void __user *name)
{
	char release[KI_UNAME_STRING_LEN];

	if (!name || ki_is_safemode())
		return 0;

	if (!ki_uname_get_effective_release(release, sizeof(release)))
		return 0;

	if (copy_to_user((char __user *)name + offsetof(struct new_utsname, release),
			 release, sizeof(release)))
		return -EFAULT;

	return 0;
}

struct ki_kfunc ki_uname_kfunc = {
	.name = "uname",
	.init = ki_uname_init,
	.exit = ki_uname_exit,
	.config_set = ki_uname_config_set,
	.config_unset = ki_uname_config_unset,
	.config_reset = ki_uname_config_reset,
	.func_set = ki_uname_func_set,
	.func_unset = ki_uname_func_unset,
	.func_reset = ki_uname_func_reset,
	.get_real = ki_uname_get_real,
};
