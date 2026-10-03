# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_KI) += ki.o

ki-y := core/ki_core.o \
	core/ki_config.o \
	core/ki_runtime.o \
	core/ki_kfunc.o \
	core/ki_device.o \
	core/ki_hook.o \
	kfunc/uname/ki_uname.o \
	kfunc/process/ki_process.o \
	kfunc/module/ki_module.o \
	kfunc/filesystem/ki_filesystem.o

ccflags-y += -I$(src)/include
ccflags-y += -I$(src)/uapi
ccflags-y += -I$(src)/kfunc/uname
ccflags-y += -I$(src)/kfunc/process
ccflags-y += -I$(src)/kfunc/module
ccflags-y += -I$(src)/kfunc/filesystem

ifneq ($(CONFIG_KI),)
ifneq ($(CONFIG_KI_TRACEPOINT_HOOK),y)
include $(src)/tools/manual_hook_check.mk
endif
endif

# Userspace is intentionally not part of this Kbuild.
