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
CONFIG_KI_BOOTAPPLY：开机时由内核读取并应用持久化配置

CONFIG_KI_TRACEPOINT_HOOK：使用 Tracepoint Syscall Redirect 作为 GKI 自动 Hook（仅适用于 GKI 2.0 / 5.10+；关闭后使用手动 Hook）

仓库默认保持 Tracepoint Hook 关闭。最小启用配置为：

```text
CONFIG_KI=y
CONFIG_KI_DEBUG=n
CONFIG_KI_BOOTAPPLY=n
CONFIG_KI_TRACEPOINT_HOOK=n
```

## Hook方法
### 手动 Hook
通过在内核里集成钩子实现Hook（适用于 3.18-6.18+ 内核）
### Tracepoint Syscall Redirect
使用 Tracepoint Syscall Redirect 实现自动 Hook（⚠️仅适用于 GKI 2.0 / 5.10+；GKI 1.0、Non-GKI 和 5.4 及以下内核请使用手动 Hook）


## Kernel Informater用户目录

配置、Debug日志和安全模式标记统一放在：

```text
/data/adb/ki_user
├── config
├── debug.log
└── safemode       # 仅在安全模式启用时存在
```

## 许可证

内核部分使用 GPL-2.0-only。UAPI 文件本身按照文件头声明采用 GPL-2.0-or-later 与 MIT 兼容的双重许可。

## 特别感谢
 * [KernelSU](https://github.com/tiann/KernelSU): 提供部分思路与参考。
 * [ReSukiSU](https://github.com/ReSukiSU/ReSukiSU): 钩子判断与setup.sh以及部分代码参考。
 * [Kasumi](https://github.com/Rouyashiki/Kasumi): Mount 管理相关思路与实现参考。

KI 是一个独立项目，具体架构与实现会根据自身需求单独开发。感谢 Android 内核开源社区提供的大量工程实践与开源参考。