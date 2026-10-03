/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/hashtable.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/task_work.h>
#include <linux/tracepoint.h>

#include <asm/syscall.h>
#include <asm/unistd.h>

#include "ki.h"
#include "ki_uname.h"
#include "ki_process.h"

#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_HAVE_SYSCALL_TRACEPOINTS)
#include <trace/events/syscalls.h>
#endif

#ifdef CONFIG_KI_TRACEPOINT_HOOK

/*
 * Android GKI 2.0 backend:
 *
 * Trace the native uname() syscall at sys_enter/sys_exit and defer the
 * userspace rewrite to task_work. The tracepoint callback never writes to
 * userspace directly, which keeps the redirect out of the tracepoint's
 * RCU/atomic execution context.
 */
struct ki_task_work {
	struct callback_head task_work;
	void __user *name;
};

static void ki_task_work_handler(struct callback_head *work)
{
	struct ki_task_work *ki_work =
		container_of(work, struct ki_task_work, task_work);

	ki_uname_apply_user_buffer(ki_work->name);
	kfree(ki_work);
}

static int ki_schedule_user_patch(void __user *name)
{
	struct ki_task_work *work;
	int ret;

	if (!name || !current->mm)
		return 0;

	work = kzalloc(sizeof(*work), GFP_ATOMIC);
	if (!work)
		return -ENOMEM;

	work->name = name;
	work->task_work.func = ki_task_work_handler;

	ret = task_work_add(current, &work->task_work, TWA_RESUME);
	if (ret) {
		kfree(work);
		return ret;
	}

	return 0;
}

#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_HAVE_SYSCALL_TRACEPOINTS)

#define KI_TP_BITS 4

struct ki_tp_call {
	struct hlist_node node;
	struct task_struct *task;
	void __user *name;
};

static DEFINE_HASHTABLE(ki_tp_calls, KI_TP_BITS);
static DEFINE_SPINLOCK(ki_tp_lock);
static bool ki_tp_registered_enter;
static bool ki_tp_registered_exit;

static void ki_tp_enter(void *unused, struct pt_regs *regs, long id)
{
	struct ki_tp_call *call;
	unsigned long args[1];
	unsigned long flags;

	(void)unused;

	if (unlikely(id != __NR_uname) || !current->mm)
		return;

#ifdef CONFIG_COMPAT
	if (is_compat_task())
		return;
#endif

	syscall_get_arguments(current, regs, args);
	if (!args[0])
		return;

	call = kmalloc(sizeof(*call), GFP_ATOMIC);
	if (!call)
		return;

	call->task = current;
	call->name = (void __user *)args[0];

	spin_lock_irqsave(&ki_tp_lock, flags);
	hash_add(ki_tp_calls, &call->node, (unsigned long)current);
	spin_unlock_irqrestore(&ki_tp_lock, flags);
}

static void ki_tp_exit(void *unused, struct pt_regs *regs, long ret)
{
	struct ki_tp_call *call = NULL;
	struct hlist_node *tmp;
	unsigned long flags;

	(void)unused;
	(void)regs;

	spin_lock_irqsave(&ki_tp_lock, flags);
	hash_for_each_possible_safe(ki_tp_calls, call, tmp, node,
				    (unsigned long)current) {
		if (call->task == current) {
			hash_del(&call->node);
			break;
		}
		call = NULL;
	}
	spin_unlock_irqrestore(&ki_tp_lock, flags);

	if (!call)
		return;

	if (!ret)
		ki_schedule_user_patch(call->name);

	kfree(call);
}

static int ki_tracepoint_hook_init(void)
{
	int ret;

	ret = register_trace_sys_enter(ki_tp_enter, NULL);
	if (ret)
		return ret;
	ki_tp_registered_enter = true;

	ret = register_trace_sys_exit(ki_tp_exit, NULL);
	if (ret) {
		unregister_trace_sys_enter(ki_tp_enter, NULL);
		ki_tp_registered_enter = false;
		tracepoint_synchronize_unregister();
		return ret;
	}
	ki_tp_registered_exit = true;

	pr_info("KI: Tracepoint Syscall Redirect hook enabled\n");
	return 0;
}

static void ki_tracepoint_hook_exit(void)
{
	struct ki_tp_call *call;
	struct hlist_node *tmp;
	unsigned long flags;
	int bkt;

	if (ki_tp_registered_exit) {
		unregister_trace_sys_exit(ki_tp_exit, NULL);
		ki_tp_registered_exit = false;
	}
	if (ki_tp_registered_enter) {
		unregister_trace_sys_enter(ki_tp_enter, NULL);
		ki_tp_registered_enter = false;
	}

	tracepoint_synchronize_unregister();

	spin_lock_irqsave(&ki_tp_lock, flags);
	hash_for_each_safe(ki_tp_calls, bkt, tmp, call, node) {
		hash_del(&call->node);
		kfree(call);
	}
	spin_unlock_irqrestore(&ki_tp_lock, flags);
}

#endif /* CONFIG_TRACEPOINTS && CONFIG_HAVE_SYSCALL_TRACEPOINTS */

int ki_hook_init(void)
{
	int ret;

	ret = ki_process_hook_init();
	if (ret) {
		pr_err("KI: process lifecycle hook initialization failed: %d\n",
		       ret);
		return ret;
	}

#ifdef CONFIG_KI_TRACEPOINT_HOOK
	ret = ki_tracepoint_hook_init();
	if (ret) {
		pr_err("KI: Tracepoint Syscall Redirect hook initialization failed: %d\n",
		       ret);
		ki_process_hook_exit();
		return ret;
	}
#else
	pr_info("KI: manual hook mode selected\n");
#endif

	return 0;
}

void ki_hook_exit(void)
{
#ifdef CONFIG_KI_TRACEPOINT_HOOK
	ki_tracepoint_hook_exit();
#endif
	ki_process_hook_exit();
}
