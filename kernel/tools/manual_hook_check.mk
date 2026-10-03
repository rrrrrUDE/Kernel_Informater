# SPDX-License-Identifier: GPL-2.0-only
#
# Kernel Informater manual-hook build-time checks.
#
# This file is included only when CONFIG_KI_KPROBEHOOK is disabled.
# Legacy/non-GKI trees must contain the source-level KI hooks inserted by
# kernel/integrate.sh.

KI_MANUAL_HOOK_UNAME_FILE := $(srctree)/kernel/sys.c
KI_MANUAL_HOOK_UNAME_SYMBOL := ki_uname_override_release

KI_MANUAL_HOOK_FORK_FILE := $(srctree)/kernel/fork.c
KI_MANUAL_HOOK_FORK_SYMBOL := ki_process_manual_fork

KI_MANUAL_HOOK_EXEC_FILE := $(srctree)/fs/exec.c
KI_MANUAL_HOOK_EXEC_SYMBOL := ki_process_manual_exec

KI_MANUAL_HOOK_EXIT_FILE := $(srctree)/kernel/exit.c
KI_MANUAL_HOOK_EXIT_SYMBOL := ki_process_manual_exit

define ki_check_manual_hook
ifeq ($$(shell grep -Fq "$(1)" "$(2)"; echo $$$$?),0)
$$(info -- Kernel Informater/manual_hook: $(1) found in $(2))
else
$$(info -- Kernel Informater/manual_hook: $(1) not found in $(2))
$$(info -- Run: kernel/integrate.sh <kernel-tree> or enable CONFIG_KI_KPROBEHOOK on a supported Android GKI tree.)
$$(error Kernel Informater requires the manual hook when CONFIG_KI_KPROBEHOOK is disabled.)
endif
endef

$(eval $(call ki_check_manual_hook,$(KI_MANUAL_HOOK_UNAME_SYMBOL),$(KI_MANUAL_HOOK_UNAME_FILE)))
$(eval $(call ki_check_manual_hook,$(KI_MANUAL_HOOK_FORK_SYMBOL),$(KI_MANUAL_HOOK_FORK_FILE)))
$(eval $(call ki_check_manual_hook,$(KI_MANUAL_HOOK_EXEC_SYMBOL),$(KI_MANUAL_HOOK_EXEC_FILE)))
$(eval $(call ki_check_manual_hook,$(KI_MANUAL_HOOK_EXIT_SYMBOL),$(KI_MANUAL_HOOK_EXIT_FILE)))
