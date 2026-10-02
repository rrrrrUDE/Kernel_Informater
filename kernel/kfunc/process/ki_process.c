/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "ki_process.h"

static int ki_process_check_access(void)
{
	return capable(CAP_SYS_PTRACE) ? 0 : -EPERM;
}

static struct task_struct *ki_process_get_task(pid_t pid)
{
	struct task_struct *task;

	if (pid <= 0)
		return NULL;

	rcu_read_lock();
	task = find_task_by_vpid(pid);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();

	return task;
}

static void ki_process_fill_entry(struct task_struct *task,
					  struct ki_ioc_process_entry *entry,
					  unsigned int index)
{
	entry->index = index;
	entry->pid = task_pid_vnr(task);
	entry->ppid = task_ppid_vnr(task);
	entry->uid = from_kuid_munged(current_user_ns(), task_uid(task));
	entry->state = task_state_to_char(task);
	get_task_comm(entry->comm, task);
}

int ki_process_list(struct ki_ioc_process_entry *entry)
{
	struct task_struct *task;
	unsigned int index = 0;

	if (!entry)
		return -EINVAL;

	read_lock(&tasklist_lock);
	for_each_process(task) {
		if (index++ == entry->index) {
			ki_process_fill_entry(task, entry, entry->index);
			read_unlock(&tasklist_lock);
			return 0;
		}
	}
	read_unlock(&tasklist_lock);

	return -ENOENT;
}

int ki_process_info(struct ki_ioc_process_info *info)
{
	struct task_struct *task;
	struct mm_struct *mm;

	if (!info || info->pid <= 0)
		return -EINVAL;
	if (ki_process_check_access())
		return -EPERM;

	task = ki_process_get_task(info->pid);
	if (!task)
		return -ESRCH;

	memset(info, 0, sizeof(*info));
	info->pid = task_pid_vnr(task);
	info->ppid = task_ppid_vnr(task);
	info->tgid = task_tgid_vnr(task);
	info->uid = from_kuid_munged(current_user_ns(), task_uid(task));
	info->gid = from_kgid_munged(current_user_ns(), task_gid(task));
	info->state = task_state_to_char(task);
	info->flags = task->flags;
	info->start_time = task->start_time;
	get_task_comm(info->comm, task);

	mm = get_task_mm(task);
	if (mm) {
		info->virtual_size = mm->total_vm << PAGE_SHIFT;
		info->resident_pages = get_mm_rss(mm);
		mmput(mm);
	}

	put_task_struct(task);
	return 0;
}

int ki_process_read_memory(struct ki_ioc_process_read *read)
{
	struct task_struct *task;
	int copied;

	if (!read || read->pid <= 0 || !read->size ||
	    read->size > KI_UAPI_PROCESS_READ_MAX)
		return -EINVAL;
	if (ki_process_check_access())
		return -EPERM;

	task = ki_process_get_task(read->pid);
	if (!task)
		return -ESRCH;

	copied = access_process_vm(task, (unsigned long)read->address,
				   read->data, read->size, 0);
	put_task_struct(task);

	if (copied <= 0)
		return -EIO;
	read->size = copied;
	return 0;
}

int ki_process_kill(pid_t pid)
{
	struct task_struct *task;
	int ret;

	if (pid <= 1)
		return -EINVAL;
	if (!capable(CAP_KILL))
		return -EPERM;

	task = ki_process_get_task(pid);
	if (!task)
		return -ESRCH;

	ret = send_sig(SIGKILL, task, 1);
	put_task_struct(task);
	return ret;
}

int ki_process_kill_tree(pid_t pid)
{
	struct task_struct *task;
	struct task_struct *parent;
	struct task_struct *child;
	struct task_struct *target;
	int ret = 0;

	if (pid <= 1)
		return -EINVAL;
	if (!capable(CAP_KILL))
		return -EPERM;

	target = ki_process_get_task(pid);
	if (!target)
		return -ESRCH;
	put_task_struct(target);

	read_lock(&tasklist_lock);
	for_each_process(task) {
		bool descendant = false;

		if (task_pid_vnr(task) == pid)
			descendant = true;
		else {
			parent = task;
			while (parent && parent->pid > 1) {
				child = parent->real_parent;
				if (!child)
					break;
				if (task_pid_vnr(child) == pid) {
					descendant = true;
					break;
				}
				if (child == parent)
					break;
				parent = child;
			}
		}

		if (descendant && task_pid_vnr(task) > 1) {
			int current_ret = send_sig(SIGKILL, task, 1);
			if (current_ret && !ret)
				ret = current_ret;
		}
	}
	read_unlock(&tasklist_lock);

	return ret;
}

static int ki_process_func_set(const char *key, const char *value)
{
	pid_t pid;

	if (!key || !value || !*value)
		return -EINVAL;

	if (kstrtoint(value, 10, &pid) || pid <= 1)
		return -EINVAL;

	if (!strcmp(key, "kill"))
		return ki_process_kill(pid);
	if (!strcmp(key, "kill_tree"))
		return ki_process_kill_tree(pid);

	return -EINVAL;
}

static int ki_process_func_unset(const char *key)
{
	return key && *key ? -EOPNOTSUPP : -EINVAL;
}

static int ki_process_func_reset(void)
{
	return 0;
}

static int ki_process_get_real(const char *key, char *value, size_t size)
{
	unsigned int count = 0;
	struct task_struct *task;

	if (!key || !value || !size)
		return -EINVAL;

	if (strcmp(key, "count"))
		return -EINVAL;

	read_lock(&tasklist_lock);
	for_each_process(task)
		count++;
	read_unlock(&tasklist_lock);

	snprintf(value, size, "%u", count);
	return 0;
}

static int ki_process_get_real_key(unsigned int index, char *key, size_t size)
{
	if (!key || !size || index != 0)
		return -ENOENT;

	strscpy(key, "count", size);
	return 0;
}

struct ki_kfunc ki_process_kfunc = {
	.name = "process",
	.func_set = ki_process_func_set,
	.func_unset = ki_process_func_unset,
	.func_reset = ki_process_func_reset,
	.get_real = ki_process_get_real,
	.get_real_key = ki_process_get_real_key,
};
