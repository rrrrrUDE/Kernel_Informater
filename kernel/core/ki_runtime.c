/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/workqueue.h>

#include "ki.h"
#include "ki_kfunc.h"

#define KI_SAFE_MODE_PATH "/data/adb/ki_user/safemode"
#define KI_SAFE_MODE_TIMEOUT_MS 2000
#define KI_SAFE_MODE_POLL_MS 100
#define KI_SAFE_MODE_POLLS (KI_SAFE_MODE_TIMEOUT_MS / KI_SAFE_MODE_POLL_MS)

static struct delayed_work ki_safemode_work;
static unsigned int ki_safemode_polls;

static bool ki_safemode_file_exists(void)
{
	struct file *file;

	file = filp_open(KI_SAFE_MODE_PATH, O_RDONLY | O_CLOEXEC, 0);
	if (IS_ERR(file))
		return false;

	filp_close(file, NULL);
	return true;
}

static void ki_safemode_workfn(struct work_struct *work)
{
	(void)work;

	if (ki_safemode_file_exists()) {
		mutex_lock(&ki_state.lock);
		ki_state.safemode = true;
		mutex_unlock(&ki_state.lock);
		pr_warn("KI: safe mode marker detected; safe mode enabled until shutdown\n");
		return;
	}

	if (++ki_safemode_polls < KI_SAFE_MODE_POLLS) {
		schedule_delayed_work(&ki_safemode_work,
				      msecs_to_jiffies(KI_SAFE_MODE_POLL_MS));
		return;
	}

	pr_info("KI: safe mode marker not detected within %u ms\n",
		KI_SAFE_MODE_TIMEOUT_MS);
}

int ki_safemode_init(void)
{
	ki_safemode_polls = 0;
	INIT_DELAYED_WORK(&ki_safemode_work, ki_safemode_workfn);
	schedule_delayed_work(&ki_safemode_work, 0);
	return 0;
}

void ki_safemode_exit(void)
{
	cancel_delayed_work_sync(&ki_safemode_work);
}

bool ki_is_safemode(void)
{
	bool value;

	mutex_lock(&ki_state.lock);
	value = ki_state.safemode;
	mutex_unlock(&ki_state.lock);
	return value;
}

int ki_func_set(const char *kfunc, const char *key, const char *value)
{
	struct ki_kfunc *func;

	if (!kfunc || !key || !value)
		return -EINVAL;
	func = ki_kfunc_find(kfunc);
	if (!func || !func->func_set || !func->func_unset || !func->func_reset)
		return -EOPNOTSUPP;
	return func->func_set(key, value);
}

int ki_func_unset(const char *kfunc, const char *key)
{
	struct ki_kfunc *func;

	if (!kfunc || !key)
		return -EINVAL;
	func = ki_kfunc_find(kfunc);
	if (!func || !func->func_set || !func->func_unset || !func->func_reset)
		return -EOPNOTSUPP;
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
	if (!func || !func->func_set || !func->func_unset || !func->func_reset)
		return -EOPNOTSUPP;
	return func->func_reset();
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
