# Kernel Informater

[中文](README.md)

A Kernel Management Tool for Android.

Kernel Informater (KI) is a kernel management tool for Android. It manages kernel information by deploying hooks within the kernel.

## Kernel Integration

## ⚠️ Kernel Informater checks every required hook. Missing any hook will cause the kernel build to fail. ⚠️

See the [Integration Documentation](integrate/README.md).

## Kconfig

CONFIG_KI: Enable Kernel Informater
CONFIG_KI_DEBUG: Enable debug mode
CONFIG_KI_BOOTAPPLY: Automatically apply the configuration at boot. (If disabled, use kicmd config active to activate it.)
CONFIG_KI_KPROBEHOOK: Use Kprobe to perform Hooking. (GKI Only. When disabled, manual Hooks will be used.)

Automatic Hooking is disabled by default. The minimum configuration is:

```text
CONFIG_KI=y
CONFIG_KI_DEBUG=n
CONFIG_KI_BOOTAPPLY=n
CONFIG_KI_KPROBEHOOK=n
```

## Hook Methods
### Manual Hook
Hooks are integrated directly into the kernel source code.

Supported kernel versions: 3.18 – 6.18+

### Kprobe Hook
Uses Kprobe to implement Hooking.

⚠️ Only supported on GKI 2.0+ kernels. Do not use this Hook method on GKI 1.0 or Non-GKI kernels.

## Kernel Informater User Directory

kicmd, configuration files, and debug logs are stored in:

```text
/data/adb/ki_userd/
```

## License
The kernel components are licensed under GPL-2.0-only.

The UAPI files are dual-licensed under GPL-2.0-or-later and MIT, as specified in their respective file headers.

## Special Thanks
- [KernelSU](https://github.com/tiann/KernelSU): Provided ideas and references for parts of the implementation.

KI is an independent project. Its architecture and implementation are developed independently according to its own requirements.

We would like to thank the Android kernel open-source community for providing a large amount of engineering experience, practical implementations, and open-source references.