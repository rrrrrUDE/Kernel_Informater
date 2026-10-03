/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/hashtable.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
#include <linux/sched/signal.h>
#endif
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tracepoint.h>
#include <linux/uaccess.h>

#include "ki_process.h"
#include "ki.h"

#define KI_PROCESS_HASH_BITS 6

struct ki_process_record {
	struct hlist_node node;
	pid_t pid;
	pid_t ppid;
	kuid_t uid;
	unsigned long state;
	char comm[TASK_COMM_LEN];
};

static DEFINE_HASHTABLE(ki_process_table, KI_PROCESS_HASH_BITS);
static DEFINE_SPINLOCK(ki_process_lock);
static atomic_t ki_process_count = ATOMIC_INIT(0);

static struct ki_process_record *ki_process_find_locked(pid_t pid)
{
	struct ki_process_record *record;

	hash_for_each_possible(ki_process_table, record, node, (unsigned long)pid) {
		if (record->pid == pid)
			return record;
	}
	return NULL;
}

static void ki_process_update_record(struct ki_process_record *record,
					struct task_struct *task)
{
	if (!record || !task)
		return;
	record->ppid = task_ppid_nr(task);
	record->uid = task_uid(task);
	record->state = task->state;
	get_task_comm(record->comm, task);
}

static void ki_process_add(struct task_struct *task)
{
	struct ki_process_record *record;
	unsigned long flags;
	pid_t pid;

	if (!task)
		return;
	pid = task_pid_vnr(task);
	if (pid <= 0)
		return;

	record = kzalloc(sizeof(*record), GFP_ATOMIC);
	if (!record)
		return;
	record->pid = pid;
	ki_process_update_record(record, task);

	spin_lock_irqsave(&ki_process_lock, flags);
	if (ki_process_find_locked(pid)) {
		spin_unlock_irqrestore(&ki_process_lock, flags);
		kfree(record);
		return;
	}
	hash_add(ki_process_table, &record->node, (unsigned long)pid);
	atomic_inc(&ki_process_count);
	spin_unlock_irqrestore(&ki_process_lock, flags);
}

static void ki_process_remove(pid_t pid)
{
	struct ki_process_record *record;
	unsigned long flags;

	spin_lock_irqsave(&ki_process_lock, flags);
	record = ki_process_find_locked(pid);
	if (record) {
		hash_del(&record->node);
		atomic_dec(&ki_process_count);
	}
	spin_unlock_irqrestore(&ki_process_lock, flags);
	kfree(record);
}

static void ki_process_clear(void)
{
	struct ki_process_record *record;
	struct hlist_node *tmp;
	unsigned long flags;
	int bkt;

	spin_lock_irqsave(&ki_process_lock, flags);
	hash_for_each_safe(ki_process_table, bkt, tmp, record, node) {
		hash_del(&record->node);
		kfree(record);
	}
	atomic_set(&ki_process_count, 0);
	spin_unlock_irqrestore(&ki_process_lock, flags);
}

#ifdef CONFIG_KI_TRACEPOINT_HOOK

static bool ki_process_hooks_registered;

static void ki_process_sched_fork(void *unused, struct task_struct *parent,
				  struct task_struct *child)
{
	(void)unused;
	(void)parent;
	ki_process_add(child);
}

static void ki_process_sched_exit(void *unused, struct task_struct *task)
{
	(void)unused;
	ki_process_remove(task_pid_vnr(task));
}

int ki_process_hook_init(void)
{
	int ret;

	ret = register_trace_sched_process_fork(ki_process_sched_fork, NULL);
	if (ret)
		return ret;

	ret = register_trace_sched_process_exit(ki_process_sched_exit, NULL);
	if (ret) {
		unregister_trace_sched_process_fork(ki_process_sched_fork, NULL);
		tracepoint_synchronize_unregister();
		return ret;
	}

	ki_process_hooks_registered = true;
	{
		struct task_struct *task;
		read_lock(&tasklist_lock);
		for_each_process(task)
			ki_process_add(task);
		read_unlock(&tasklist_lock);
	}
	return 0;
}

void ki_process_hook_exit(void)
{
	if (ki_process_hooks_registered) {
		unregister_trace_sched_process_exit(ki_process_sched_exit, NULL);
		unregister_trace_sched_process_fork(ki_process_sched_fork, NULL);
		tracepoint_synchronize_unregister();
		ki_process_hooks_registered = false;
	}
	ki_process_clear();
}

#else

int ki_process_hook_init(void)
{
	struct task_struct *task;

	read_lock(&tasklist_lock);
	for_each_process(task)
		ki_process_add(task);
	read_unlock(&tasklist_lock);
	return 0;
}

void ki_process_hook_exit(void)
{
	ki_process_clear();
}

#endif

void ki_process_manual_fork(struct task_struct *task)
{
	ki_process_add(task);
}

void ki_process_manual_exec(struct task_struct *task)
{
	struct ki_process_record *record;
	unsigned long flags;

	if (!task)
		return;

	spin_lock_irqsave(&ki_process_lock, flags);
	record = ki_process_find_locked(task_pid_nr(task));
	if (record)
		ki_process_update_record(record, task);
	spin_unlock_irqrestore(&ki_process_lock, flags);
}

void ki_process_manual_exit(struct task_struct *task)
{
	if (task)
		ki_process_remove(task_pid_nr(task));
}

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

int ki_process_list(struct ki_ioc_process_entry *entry)
{
	struct ki_process_record *record;
	struct hlist_node *tmp;
	unsigned int index = 0;
	unsigned long flags;
	int bkt;
	int ret = -ENOENT;

	if (!entry)
		return -EINVAL;

	spin_lock_irqsave(&ki_process_lock, flags);
	hash_for_each_safe(ki_process_table, bkt, tmp, record, node) {
		if (index++ == entry->index) {
			entry->index = index - 1;
			entry->pid = record->pid;
			entry->ppid = record->ppid;
			entry->uid = from_kuid_munged(current_user_ns(), record->uid);
			entry->state = record->state;
			strscpy(entry->comm, record->comm, sizeof(entry->comm));
			ret = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&ki_process_lock, flags);
	return ret;
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
	info->ppid = task_ppid_nr(task);
	info->tgid = task_tgid_vnr(task);
	info->uid = from_kuid_munged(current_user_ns(), task_uid(task));
	info->gid = from_kgid_munged(current_user_ns(), __task_cred(task)->gid);
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
	if (!key || !value || !size)
		return -EINVAL;
	if (strcmp(key, "count"))
		return -EINVAL;

	snprintf(value, size, "%d", atomic_read(&ki_process_count));
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
