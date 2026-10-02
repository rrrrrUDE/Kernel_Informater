# Kernel Informater

[简体中文](README_cn.md)

A Kernel Management Tool for Android.

Kernel Informater (KI) is a kernel-side management framework for Android/Linux kernels. It provides kernel information control through `kfunc`, a shared kernel/userspace UAPI, persistent userspace configuration, temporary runtime controls, safe mode, and optional GKI hook backends.

## Repository layout

```text
Kernel-Informater/
├── Documentation/
│   ├── README.md
│   ├── README_cn.md
│   └── integrate/
│       ├── README.md
│       └── README_cn.md
├── kernel/
│   ├── Kconfig
│   ├── Makefile
│   ├── setup.sh
│   ├── integrate.sh
│   ├── include/
│   ├── uapi -> ../uapi
│   ├── core/
│   ├── kfunc/
│   └── tools/
├── uapi/
│   └── ki_uapi.h
└── userspace/
    └── kicmd/
        ├── kicmd.c
        └── kicmd_def.h
```

The userspace CLI is not part of kernel Kbuild and intentionally has no Makefile. It is compiled separately against the exact same `uapi/ki_uapi.h` used by the kernel side.

## Integration

Run the setup script from the target kernel source root:

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh
```

When the script is run interactively, it asks whether the automatic GKI hook backend is wanted. Answer `y` for automatic tracepoint/kprobe hooks. Press Enter or answer `n` to use the source-level manual integration path through `kernel/integrate.sh`.

Equivalent explicit modes are:

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
sh /path/to/Kernel-Informater/kernel/setup.sh --manual-hook
```

The repository kernel directory is exposed in the target tree as:

```text
kernel/Kernel_Informater -> /path/to/Kernel-Informater/kernel
```

Inside that tree, the UAPI path is:

```text
kernel/Kernel_Informater/uapi -> /path/to/Kernel-Informater/uapi
```

This makes the kernel and `kicmd` consume one UAPI definition.

Cleanup:

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --cleanup
```

See [Integration Guide](integrate/README.md) for the full integration flow.

## Kconfig

The repository baseline keeps automatic hooks disabled. A minimal enabled configuration is:

```text
CONFIG_KI=y
CONFIG_KI_DEBUG=n
CONFIG_KI_BOOTAPPLY=n
CONFIG_KI_KPROBEHOOK=n
```

`setup.sh --auto-hook` temporarily changes the repository `Kconfig` marker for `CONFIG_KI_KPROBEHOOK` from its baseline default `n` to `y`. Cleanup or manual mode restores the baseline `n`.

## Hook backends

### Manual hook

`kernel/integrate.sh` performs source-level integration for legacy or non-GKI trees. It hooks the userspace-visible `uname` release value immediately after the existing `override_release()` call sites and covers every matching native site found in `kernel/sys.c`.

### Automatic GKI hook

When `CONFIG_KI_KPROBEHOOK=y`, KI first tries the raw syscall tracepoint API exposed by modern GKI kernels and falls back to arm64 syscall-wrapper kretprobes when the tracepoint backend is unavailable.

The automatic implementation is intentionally based on modern GKI-style mechanisms used by Android kernels in the 5.10/6.1 generation. It does not use pre-5.4 NGKI syscall hooking techniques.

For the automatic backend, native AArch64 `uname` is handled. Compat 32-bit tasks are skipped in order to avoid applying a native `new_utsname` layout to a compat syscall buffer. Legacy/non-GKI trees can use the manual integration path instead.

## kicmd

`userspace/kicmd/` contains the CLI source only. It is not built into the kernel and has no userspace Makefile.

Build it separately, for example:

```bash
cd /path/to/Kernel-Informater/userspace/kicmd
cc -Wall -Wextra -I../../uapi kicmd.c -o kicmd
```

The CLI communicates with the kernel through `/dev/ki` and the shared UAPI.

```text
kicmd
├── safemode
├── config
│   ├── del
│   ├── set
│   ├── unset
│   ├── reset
│   ├── active
│   ├── inactive
│   └── list
├── list
├── func
│   ├── set
│   ├── unset
│   └── reset
├── help
└── version
```

Effective precedence is:

```text
func (temporary runtime)
        >
config (persistent + active)
        >
real kernel information
```

`kicmd list` always requests the real information from the kernel and does not report the effective overridden value.

## KI userspace state

All KI userspace persistent state and debug files are kept under:

```text
/data/adb/ki_userd/
├── config
├── debug
└── debug.log
```

`config` stores persistent configuration. The `debug` marker enables CLI debug logging, which is appended to `debug.log`.

The directory is created with restrictive permissions by `kicmd` when required.

## Build-time hook checking

When `CONFIG_KI_KPROBEHOOK=y` is enabled, the GKI tracepoint/kprobe backend is used and the manual source-hook check is skipped.

When `CONFIG_KI_KPROBEHOOK` is disabled, `kernel/tools/manual_hook_check.mk` is included by Kbuild. It checks `kernel/sys.c` during Kbuild parsing and stops the build when the manual KI uname hook is missing.

This keeps the check in the normal kernel build path and avoids a separate manual checker. To validate only KI instead of the whole kernel, build the KI directory as a kernel module with `M=kernel/Kernel_Informater`.

## License

The kernel part is released under GPL-2.0-only. The UAPI header is dual-licensed under GPL-2.0-or-later and MIT-compatible terms as indicated in the file itself.

## Contributing and Credits

Ideas and implementation references for this project are drawn from established Android kernel projects. In particular, [KernelSU](https://github.com/tiann/KernelSU) by `tiann` provides part of the ideas used in the project, especially around kernel/userspace separation, kernel integration, and kernel-side interfaces.

KI is an independent project and its architecture and implementation are developed separately. Thanks to the open-source Android kernel community for the engineering references and prior art that make this work possible.
