/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "ki_kfunc.h"

struct ki_kfunc_node {
	struct list_head list;
	struct hlist_node hash;
	struct ki_kfunc *kfunc;
};

static LIST_HEAD(ki_kfunc_list);
static DEFINE_MUTEX(ki_kfunc_lock);

static struct ki_kfunc *ki_kfunc_find_locked(const char *name)
{
	struct ki_kfunc_node *node;

	list_for_each_entry(node, &ki_kfunc_list, list) {
		if (!strcmp(node->kfunc->name, name))
			return node->kfunc;
	}

	return NULL;
}

struct ki_kfunc *ki_kfunc_find(const char *name)
{
	struct ki_kfunc *kfunc;

	if (!name)
		return NULL;

	mutex_lock(&ki_kfunc_lock);
	kfunc = ki_kfunc_find_locked(name);
	mutex_unlock(&ki_kfunc_lock);

	return kfunc;
}

struct ki_kfunc *ki_kfunc_find_by_index(unsigned int index)
{
	struct ki_kfunc_node *node;
	struct ki_kfunc *kfunc = NULL;
	unsigned int i = 0;

	mutex_lock(&ki_kfunc_lock);
	list_for_each_entry(node, &ki_kfunc_list, list) {
		if (i++ == index) {
			kfunc = node->kfunc;
			break;
		}
	}
	mutex_unlock(&ki_kfunc_lock);

	return kfunc;
}

int ki_kfunc_reset_all_config(void)
{
	struct ki_kfunc_node *node;
	int ret = 0;

	mutex_lock(&ki_kfunc_lock);
	list_for_each_entry(node, &ki_kfunc_list, list) {
		if (node->kfunc->config_reset) {
			int current_ret = node->kfunc->config_reset();
			if (current_ret && !ret)
				ret = current_ret;
		}
	}
	mutex_unlock(&ki_kfunc_lock);

	return ret;
}

int ki_kfunc_reset_all_func(void)
{
	struct ki_kfunc_node *node;
	int ret = 0;

	mutex_lock(&ki_kfunc_lock);
	list_for_each_entry(node, &ki_kfunc_list, list) {
		if (node->kfunc->func_reset) {
			int current_ret = node->kfunc->func_reset();
			if (current_ret && !ret)
				ret = current_ret;
		}
	}
	mutex_unlock(&ki_kfunc_lock);

	return ret;
}

int ki_kfunc_register(struct ki_kfunc *kfunc)
{
	struct ki_kfunc_node *node;
	int ret = 0;

	if (!kfunc || !kfunc->name)
		return -EINVAL;

	mutex_lock(&ki_kfunc_lock);

	if (ki_kfunc_find_locked(kfunc->name)) {
		ret = -EEXIST;
		goto out_unlock;
	}

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	node->kfunc = kfunc;
	list_add_tail(&node->list, &ki_kfunc_list);

out_unlock:
	mutex_unlock(&ki_kfunc_lock);

	if (ret)
		return ret;

	if (kfunc->init) {
		ret = kfunc->init();
		if (ret)
			ki_kfunc_unregister(kfunc);
	}

	return ret;
}

int ki_kfunc_unregister(struct ki_kfunc *kfunc)
{
	struct ki_kfunc_node *node;
	struct ki_kfunc_node *tmp;
	bool found = false;

	if (!kfunc)
		return -EINVAL;

	mutex_lock(&ki_kfunc_lock);

	list_for_each_entry_safe(node, tmp, &ki_kfunc_list, list) {
		if (node->kfunc == kfunc) {
			list_del(&node->list);
			hash_del(&node->hash);
			kfree(node);
			found = true;
			break;
		}
	}

	mutex_unlock(&ki_kfunc_lock);

	if (!found)
		return -ENOENT;

	if (kfunc->exit)
		kfunc->exit();

	return 0;
}
