# Kernel Informater

[English](README_en.md)

A Kernel Management Tool for Android.

Kernel Informater（KI）是一个面向 Android 的内核管理工具，通过在内核里部署钩子来管理内核信息。

## 内核集成

## ⚠️Kernel Informater会检查每一个钩子，如果缺少钩子会导致编译失败⚠️
请看[集成文档](integrate/README.md)。

## Kconfig

CONFIG_KI：启用Kernel Informater
CONFIG_KI_DEBUG:开启调试模式
CONFIG_KI_BOOTAPPLY：开机时自动应用配置（未启用需使用kicmd config active来激活）
CONFIG_KI_KPROBEHOOK：使用Kprobe来Hook(GKI Only ,关闭后会使用手动钩子来Hook）

仓库默认保持自动 Hook 关闭。最小启用配置为：

```text
CONFIG_KI=y
CONFIG_KI_DEBUG=n
CONFIG_KI_BOOTAPPLY=n
CONFIG_KI_KPROBEHOOK=n
```

## Hook方法
### 手动 Hook
通过在内核里集成钩子实现Hook（适用于 3.18-6.18+ 内核）
### Kprobe Hook
使用Kprobe实现Hook（⚠️仅适用于 GKI 2.0+ 内核，GKI 1.0 或 Non-GKI请勿使用该钩子）


## Kernel Informater用户目录

kicmd、配置和Debug日志会统一放在：

```text
/data/adb/ki_userd/
```

## 许可证

内核部分使用 GPL-2.0-only。UAPI 文件本身按照文件头声明采用 GPL-2.0-or-later 与 MIT 兼容的双重许可。

## 特别感谢
 * [KernelSU](https://github.com/tiann/KernelSU): 提供部分思路与参考。
 * [ReSukiSU](https://github.com/ReSukiSU/ReSukiSU): 钩子判断与setup.sh参考。

KI 是一个独立项目，具体架构与实现会根据自身需求单独开发。感谢 Android 内核开源社区提供的大量工程实践与开源参考。
