# Kernel Informater

[中文](README.md)

A Kernel Management Tool for Android.

Kernel Informater (KI) is a kernel management tool for Android. It manages kernel by deploying hooks within the kernel.

## Kernel Integration

## ⚠️ Kernel Informater checks every required hook. Missing any hook will cause the kernel build to fail. ⚠️

See the [Integration Documentation](integrate/README_en.md).

## Kconfig

CONFIG_KI: Enable Kernel Informater
CONFIG_KI_DEBUG: Enable debug mode
CONFIG_KI_BOOTAPPLY: Let the kernel read and apply persistent configuration at boot.
CONFIG_KI_TRACEPOINT_HOOK: Enable Tracepoint Syscall Redirect as the automatic GKI Hook. (GKI 2.0 / 5.10+ only; manual hooks remain available.)

Tracepoint Syscall Redirect Hooking is disabled by default. The minimum configuration is:

```text
CONFIG_KI=y
CONFIG_KI_DEBUG=n
CONFIG_KI_BOOTAPPLY=n
CONFIG_KI_TRACEPOINT_HOOK=n
```

## Hook Methods
### Manual Hook
Hooks are integrated directly into the kernel source code.

Supported kernel generations: legacy/GKI 1.0/non-GKI kernels, including 5.4 and below; GKI 2.0+ also supports manual integration.

### Tracepoint Syscall Redirect Hook
Uses Tracepoint Syscall Redirect to implement Hooking.

⚠️ Supported for Android GKI 2.0+ kernels (5.10+). GKI 1.0, non-GKI kernels, and 5.4-or-older kernels must use Manual Hook.

The setup script reads `VERSION` and `PATCHLEVEL` directly from the target kernel root `Makefile` and only deploys the automatic Tracepoint Hook when the detected version is 5.10 or newer. Older kernels are kept on the Manual Hook path.

## Kernel Informater User Directory

Persistent configuration, debug logs, and the safe-mode marker are stored in:

```text
/data/adb/ki_user
├── config
├── debug.log
└── safemode       # exists only while safe mode is enabled
```

## License
The kernel components are licensed under GPL-2.0-only.

The UAPI files are dual-licensed under GPL-2.0-or-later and MIT, as specified in their respective file headers.

## Special Thanks
 * [KernelSU](https://github.com/tiann/KernelSU): Provides some ideas and code references.
 * [BakaSU](https://github.com/Baka-SU/BakaSU): Provides partial code and Non-GKI support code references.
 * [Kasumi](https://github.com/Rouyashiki/Kasumi) + [NoMount](https://github.com/maxsteeel/nomount): Provides references for mount management concepts and implementations.

KI is an independent project. Its architecture and implementation are developed independently according to its own requirements.

We would like to thank the Android kernel open-source community for providing a large amount of engineering experience, practical implementations, and open-source references.
