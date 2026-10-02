/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/hashtable.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/task_work.h>
#include <linux/tracepoint.h>

#include <asm/ptrace.h>
#include <asm/unistd.h>

#include "ki.h"
#include "ki_uname.h"

#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_HAVE_SYSCALL_TRACEPOINTS)
#include <trace/events/syscalls.h>
#endif

#if defined(CONFIG_KPROBES) && defined(CONFIG_KRETPROBES)
#include <linux/kprobes.h>
#endif

#ifdef CONFIG_KI_KPROBEHOOK

/*
 * Modern Android GKI backend:
 *
 * - Prefer raw syscall tracepoints (sys_enter/sys_exit).
 * - Fall back to arm64 syscall-wrapper kretprobes.
 *
 * The callback never writes userspace memory directly. It records the uname
 * destination pointer and schedules task_work so the final copy happens in
 * normal task context immediately before returning to userspace.
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
	unsigned long flags;
	unsigned long name;

	(void)unused;

	if (id != __NR_uname || !current->mm)
		return;

#ifdef CONFIG_COMPAT
	if (is_compat_task())
		return;
#endif

	/* Native arm64 GKI uname() receives its output pointer in x0. */
	name = regs->regs[0];
	if (!name)
		return;

	call = kmalloc(sizeof(*call), GFP_ATOMIC);
	if (!call)
		return;

	call->task = current;
	call->name = (void __user *)name;

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

static int ki_tracepoint_init(void)
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

	pr_info("KI: GKI raw syscall tracepoint hook enabled\n");
	return 0;
}

static void ki_tracepoint_exit(void)
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

static bool ki_tracepoint_backend;

#else
static inline int ki_tracepoint_init(void)
{
	return -EOPNOTSUPP;
}

static inline void ki_tracepoint_exit(void)
{
}

static bool ki_tracepoint_backend;
#endif

#if defined(CONFIG_KPROBES) && defined(CONFIG_KRETPROBES)

struct ki_kret_data {
	void __user *name;
};

static int ki_kret_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct ki_kret_data *data = ri->data;
	struct pt_regs *sys_regs;

	if (!current->mm)
		return 1;

#ifdef CONFIG_COMPAT
	if (is_compat_task())
		return 1;
#endif

	/* GKI arm64 syscall wrappers receive struct pt_regs * in x0. */
	sys_regs = (struct pt_regs *)regs->regs[0];
	if (!sys_regs)
		return 1;

	/* GKI syscall wrappers pass syscall arguments in regs->regs[0..5]. */
	data->name = (void __user *)sys_regs->regs[0];
	return data->name ? 0 : 1;
}

static int ki_kret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct ki_kret_data *data = ri->data;
	long ret = regs_return_value(regs);

	if (!ret && data->name)
		return ki_schedule_user_patch(data->name);

	return 0;
}

static struct kretprobe ki_kretprobes[] = {
	{
		.entry_handler = ki_kret_entry,
		.handler = ki_kret_handler,
		.data_size = sizeof(struct ki_kret_data),
		.maxactive = 64,
	},
	{
		.entry_handler = ki_kret_entry,
		.handler = ki_kret_handler,
		.data_size = sizeof(struct ki_kret_data),
		.maxactive = 64,
	},
};

static const char * const ki_kprobe_symbols[] = {
	"__arm64_sys_uname",
	"__arm64_sys_newuname",
};

static bool ki_kprobe_registered[ARRAY_SIZE(ki_kretprobes)];
static bool ki_kprobe_backend;

static int ki_kprobe_init(void)
{
	int i;
	int ret;
	int registered = 0;

	for (i = 0; i < ARRAY_SIZE(ki_kretprobes); ++i) {
		ki_kretprobes[i].kp.symbol_name = ki_kprobe_symbols[i];
		ret = register_kretprobe(&ki_kretprobes[i]);
		if (!ret) {
			ki_kprobe_registered[i] = true;
			registered++;
			pr_info("KI: GKI kretprobe installed on %s\n",
				ki_kprobe_symbols[i]);
		} else if (ret != -ENOENT) {
			pr_warn("KI: GKI kretprobe %s failed: %d\n",
				ki_kprobe_symbols[i], ret);
		}
	}

	if (!registered)
		return -ENOENT;

	return 0;
}

static void ki_kprobe_exit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ki_kretprobes); ++i) {
		if (ki_kprobe_registered[i]) {
			unregister_kretprobe(&ki_kretprobes[i]);
			ki_kprobe_registered[i] = false;
		}
	}
}

#else
static inline int ki_kprobe_init(void)
{
	return -EOPNOTSUPP;
}

static inline void ki_kprobe_exit(void)
{
}

static bool ki_kprobe_backend;
#endif

int ki_hook_init(void)
{
	int ret;

	if (!IS_ENABLED(CONFIG_KI_KPROBEHOOK)) {
		pr_info("KI: manual hook mode selected\n");
		return 0;
	}

	ret = ki_tracepoint_init();
	if (!ret) {
		ki_tracepoint_backend = true;
		return 0;
	}

	pr_warn("KI: GKI tracepoint hook unavailable: %d; trying kprobe\n", ret);
	ret = ki_kprobe_init();
	if (!ret) {
		ki_kprobe_backend = true;
		return 0;
	}

	pr_err("KI: no GKI automatic hook backend available: %d\n", ret);
	return ret;
}

void ki_hook_exit(void)
{
	if (ki_tracepoint_backend) {
		ki_tracepoint_exit();
		ki_tracepoint_backend = false;
	}

	if (ki_kprobe_backend) {
		ki_kprobe_exit();
		ki_kprobe_backend = false;
	}
}

#else /* !CONFIG_KI_KPROBEHOOK */

int ki_hook_init(void)
{
	pr_info("KI: manual hook mode selected\n");
	return 0;
}

void ki_hook_exit(void)
{
}

#endif /* CONFIG_KI_KPROBEHOOK */
