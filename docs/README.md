# Kernel Informater

[English](README_en.md)

A Kernel Management Tool for Android.

Kernel Informater（KI）是一个面向 Android/Linux 内核的内核侧管理框架，用于通过 `kfunc` 管理内核信息，提供统一的内核/用户空间 UAPI、持久化配置、临时运行时修改、Safe Mode，以及可选的 GKI Hook 后端。

## 仓库结构

```text
Kernel-Informater/
├── Documentation/
│   ├── README.md
│   ├── README_en.md
│   └── integrate/
│       ├── README.md
│       └── README_en.md
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

用户空间 CLI 不属于内核 Kbuild，并且刻意不提供 Makefile。内核端和 `kicmd` 均使用同一个 `uapi/ki_uapi.h`。

## 内核集成

在目标内核源码根目录运行：

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh
```

交互式运行时，`setup.sh` 会询问是否启用自动 GKI Hook。输入 `y` 使用自动 tracepoint/kprobe Hook；直接回车或输入 `n` 则默认执行 `kernel/integrate.sh`，进行源码级手动 Hook。

也可以显式指定：

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
sh /path/to/Kernel-Informater/kernel/setup.sh --manual-hook
```

目标内核中会形成：

```text
kernel/Kernel_Informater -> /path/to/Kernel-Informater/kernel
```

同时仓库内：

```text
kernel/uapi -> ../uapi
```

因此目标内核中的：

```text
kernel/Kernel_Informater/uapi
```

最终与仓库根目录 `uapi/` 指向同一份 UAPI。

清理：

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --cleanup
```

详细说明见[集成文档](integrate/README.md)。

## Kconfig

仓库默认保持自动 Hook 关闭。最小启用配置为：

```text
CONFIG_KI=y
CONFIG_KI_DEBUG=n
CONFIG_KI_BOOTAPPLY=n
CONFIG_KI_KPROBEHOOK=n
```

执行 `setup.sh --auto-hook` 时，会把仓库 `Kconfig` 中标记为 `KI_AUTO_HOOK_DEFAULT` 的 `CONFIG_KI_KPROBEHOOK` 默认值临时改成 `y`。使用 cleanup 或手动模式后恢复为 `n`。

## Hook 后端

### 手动 Hook

`kernel/integrate.sh` 面向旧内核或非 GKI 内核树进行源码级集成。它会在 `kernel/sys.c` 中现有 `override_release()` 调用点之后加入 KI 的 `uname` release Hook，并处理脚本实际找到的全部匹配位置。

### 自动 GKI Hook

启用 `CONFIG_KI_KPROBEHOOK=y` 后，KI 优先使用现代 GKI 内核提供的 raw syscall tracepoint API；当 tracepoint 后端不可用时，再回退到 arm64 syscall wrapper kretprobe。

自动 Hook 按 Android GKI 5.10/6.1 代际代码风格设计，不采用 5.4 以下 NGKI 的 syscall Hook 方法。

当前自动后端处理原生 AArch64 `uname`。Compat 32 位任务会被跳过，以避免把原生 `new_utsname` 缓冲区布局错误地应用于 compat syscall。旧版/非 GKI 内核建议使用手动集成方式。

## kicmd

`userspace/kicmd/` 只保存用户空间 CLI 源码，不参与内核编译，也没有 Makefile。

例如：

```bash
cd /path/to/Kernel-Informater/userspace/kicmd
cc -Wall -Wextra -I../../uapi kicmd.c -o kicmd
```

`kicmd` 通过 `/dev/ki` 和共享 UAPI 与内核通信。

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

实际生效优先级：

```text
func（临时运行时）
        >
config（持久化且 active）
        >
真实内核信息
```

`kicmd list` 始终从内核请求修改前的真实信息，不显示 KI 覆盖后的 effective 值。

## KI 用户空间基础目录

所有 KI 用户空间持久化配置和 Debug 状态统一放在：

```text
/data/adb/ki_userd/
├── config
├── debug
└── debug.log
```

其中 `config` 保存持久配置；创建 `debug` 标记后，`kicmd` 会把 Debug 信息追加到 `debug.log`。

## 编译期 Hook 检测

当启用 `CONFIG_KI_KPROBEHOOK=y` 时，使用 GKI tracepoint/kprobe 自动 Hook，手动源码 Hook 检测会跳过。

当未启用 `CONFIG_KI_KPROBEHOOK` 时，Kbuild 会自动包含 `kernel/tools/manual_hook_check.mk`，在解析内核构建规则时检查 `kernel/sys.c`。如果没有找到 KI `uname` 手动 Hook，就会直接终止编译。

这样检测属于正常内核编译流程，不再需要额外手动执行检测脚本。若只想检测/编译 KI，而不是完整内核，可使用 `M=kernel/Kernel_Informater` 的模块构建方式。

## 许可证

内核部分使用 GPL-2.0-only。UAPI 文件本身按照文件头声明采用 GPL-2.0-or-later 与 MIT 兼容的双重许可。

## 贡献与致谢

本项目的部分设计思路参考了已有的 Android 内核开源项目，特别感谢 [`tiann/KernelSU`](https://github.com/tiann/KernelSU) 提供的部分思路与参考，包括内核/用户空间分离、内核集成方式以及内核侧接口设计等方面。

KI 是一个独立项目，具体架构与实现会根据自身需求单独开发。感谢 Android 内核开源社区提供的大量工程实践与开源参考。
