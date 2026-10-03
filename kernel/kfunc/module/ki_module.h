/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KI_MODULE_H
#define _KI_MODULE_H
#include "ki_kfunc.h"
extern struct ki_kfunc ki_module_kfunc;
int ki_module_list_line(unsigned int index, char *line, size_t size);
#endif
