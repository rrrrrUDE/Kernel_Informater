# Kernel Informater Integration

## Setup

Run the script from the root of the target kernel source tree:

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh
```

Without an explicit hook option, setup asks:

```text
Enable automatic GKI tracepoint/kprobe hook? [y/N]:
```

`y` selects the automatic GKI backend. Any other answer, including Enter, selects the manual source integration path and runs `kernel/integrate.sh`.

Explicit modes:

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
sh /path/to/Kernel-Informater/kernel/setup.sh --manual-hook
```

## Symlink layout

The setup creates:

```text
<kernel-tree>/kernel/Kernel_Informater
    -> <KI-repository>/kernel
```

The KI repository itself contains:

```text
<repo>/kernel/uapi
    -> <repo>/uapi
```

Therefore the target kernel sees the same shared UAPI at:

```text
<kernel-tree>/kernel/Kernel_Informater/uapi
    -> <KI-repository>/uapi
```

No second copy of the UAPI is created.

## Kbuild/Kconfig integration

The target kernel gets these entries once:

```text
source "kernel/Kernel_Informater/Kconfig"
```

in `kernel/Kconfig`, and:

```make
obj-$(CONFIG_KI) += Kernel_Informater/
```

in `kernel/Makefile`.

The KI source remains outside the target kernel Git worktree and is exposed through the symlink, which keeps development and kernel integration separate.

## Automatic GKI Hook

`--auto-hook` changes the marked baseline line in KI's `kernel/Kconfig`:

```text
default n # KI_AUTO_HOOK_DEFAULT
```

to:

```text
default y # KI_AUTO_HOOK_DEFAULT
```

With automatic Hook enabled, setup does not run `kernel/integrate.sh`.

The kernel implementation prefers:

```text
raw syscall tracepoint
        ↓
arm64 syscall-wrapper kretprobe fallback
```

The tracepoint path follows the modern GKI syscall tracepoint API (`sys_enter` / `sys_exit`). The kprobe fallback targets GKI-style arm64 syscall wrapper symbols and uses a return probe plus task_work to update the userspace result after the syscall has completed.

The current automatic backend is intended for Android GKI 5.10/6.1-style kernels. It skips compat 32-bit tasks and only implements native AArch64 `uname` in this initial version.

## Manual Hook

Manual integration is intended for legacy/non-GKI kernels and for trees where the automatic symbols or tracepoints are unavailable.

```bash
sh /path/to/Kernel-Informater/kernel/integrate.sh /path/to/kernel
```

The script creates these target header links:

```text
include/linux/ki.h
include/linux/ki_kfunc.h
include/linux/ki_uname.h
```

It then inserts the KI Hook after every matching native call of:

```c
if (override_release(name->release, sizeof(name->release)))
	return -EFAULT;
```

The operation is marker-based and idempotent. The script refuses to guess when no expected hook site exists.

Review the result before compiling:

```bash
git diff -- kernel/sys.c include/linux
```

## Build checking

The standalone kernel build checker lives in `kernel/tools/`:

```bash
sh /path/to/Kernel-Informater/kernel/tools/build_check.sh
```

Automatic backend:

```bash
sh /path/to/Kernel-Informater/kernel/tools/build_check.sh --auto-hook
```

The checker builds the KI subtree as a temporary module configuration and can cross-check undefined module symbols against `vmlinux` using `kernel/tools/check_symbol`.

## Cleanup

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh --cleanup
```

Cleanup removes the target `Kernel_Informater` symlink, target Kbuild/Kconfig lines, manual include links, manual `sys.c` hook blocks, and restores the KI automatic-hook default marker to `n`.
