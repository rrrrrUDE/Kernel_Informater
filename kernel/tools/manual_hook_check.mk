# SPDX-License-Identifier: GPL-2.0-only
#
# Kernel Informater manual-hook build-time checks.
#
# This file is included only when CONFIG_KI_KPROBEHOOK is not enabled.
# In that mode the target kernel must contain the source-level KI uname hooks
# inserted by kernel/integrate.sh.

KI_MANUAL_HOOK_FILE := $(srctree)/kernel/sys.c
KI_MANUAL_HOOK_SYMBOL := ki_uname_override_release

# Keep this check at Kbuild parse time so a manually integrated kernel fails
# early instead of producing a kernel with CONFIG_KI enabled but no hook.
define ki_check_manual_hook
ifeq ($$(shell grep -Fq "$(1)" "$(2)"; echo $$$$?),0)
$$(info -- Kernel Informater/manual_hook: $(1) found in $(2))
else
$$(info -- Kernel Informater/manual_hook: $(1) not found in $(2))
$$(info -- Run: kernel/integrate.sh <kernel-tree> or use CONFIG_KI_KPROBEHOOK=y for GKI auto hooks.)
$$(error Kernel Informater requires the manual uname hook when CONFIG_KI_KPROBEHOOK is disabled.)
endif
endef

ifneq ($(wildcard $(KI_MANUAL_HOOK_FILE)),)
$(eval $(call ki_check_manual_hook,$(KI_MANUAL_HOOK_SYMBOL),$(KI_MANUAL_HOOK_FILE)))
else
$(error Kernel Informater cannot find $(KI_MANUAL_HOOK_FILE))
endif
