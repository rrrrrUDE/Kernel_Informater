/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_PROCESS_H
#define _KI_PROCESS_H

#include "ki_kfunc.h"

struct ki_ioc_process_entry;
struct ki_ioc_process_info;
struct ki_ioc_process_read;

extern struct ki_kfunc ki_process_kfunc;

int ki_process_hook_init(void);
void ki_process_hook_exit(void);
void ki_process_manual_fork(struct task_struct *task);
void ki_process_manual_exec(struct task_struct *task);
void ki_process_manual_exit(struct task_struct *task);
int ki_process_list(struct ki_ioc_process_entry *entry);
int ki_process_info(struct ki_ioc_process_info *info);
int ki_process_read_memory(struct ki_ioc_process_read *read);
int ki_process_kill(pid_t pid);
int ki_process_kill_tree(pid_t pid);

#endif /* _KI_PROCESS_H */
