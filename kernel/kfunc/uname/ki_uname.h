/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_UNAME_H
#define _KI_UNAME_H

#include <linux/types.h>
#include "ki_kfunc.h"

extern struct ki_kfunc ki_uname_kfunc;

int ki_uname_override_release(char *release, size_t size);
int ki_uname_apply_user_buffer(void __user *name);

#endif /* _KI_UNAME_H */
