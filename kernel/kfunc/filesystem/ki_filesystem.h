/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_FILESYSTEM_H
#define _KI_FILESYSTEM_H

#include "ki_kfunc.h"

extern struct ki_kfunc ki_filesystem_kfunc;
int ki_filesystem_list_line(unsigned int index, char *line, size_t size);
long ki_filesystem_mount(const struct ki_ioc_filesystem_mount *request);

#endif
