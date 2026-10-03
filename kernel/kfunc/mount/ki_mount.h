/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_MOUNT_H
#define _KI_MOUNT_H
#include "ki_kfunc.h"
extern struct ki_kfunc ki_mount_kfunc;
int ki_mount_list_line(unsigned int index, char *line, size_t size);
#endif
