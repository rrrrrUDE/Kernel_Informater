# Kernel Informater 内核集成

## 初始化

在目标内核源码根目录执行：

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh
```

不指定 Hook 参数时，脚本会询问：

```text
Enable automatic GKI tracepoint/kprobe hook? [y/N]:
```

输入 `y` 使用自动 GKI Hook；其他任何输入（包括直接回车）都会选择手动源码集成，并执行 `kernel/integrate.sh`。

也可以显式指定：

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
sh /path/to/Kernel-Informater/kernel/setup.sh --manual-hook
```

## 软链接结构

初始化后目标内核会有：

```text
<kernel-tree>/kernel/Kernel_Informater
    -> <KI-repository>/kernel
```

仓库本身则保持：

```text
<repo>/kernel/uapi
    -> <repo>/uapi
```

因此目标内核看到的：

```text
<kernel-tree>/kernel/Kernel_Informater/uapi
    -> <KI-repository>/uapi
```

内核端与 `kicmd` 使用同一份 UAPI，不产生第二份复制文件。

## Kbuild/Kconfig

初始化脚本会向目标内核添加：

```text
source "kernel/Kernel_Informater/Kconfig"
```

到 `kernel/Kconfig`，以及：

```make
obj-$(CONFIG_KI) += Kernel_Informater/
```

到 `kernel/Makefile`。

KI 自身源码仍然保存在独立仓库中，通过软链接进入目标内核源码树。

## 自动 GKI Hook

执行：

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
```

会把 KI `kernel/Kconfig` 中带有 `KI_AUTO_HOOK_DEFAULT` 标记的：

```text
default n # KI_AUTO_HOOK_DEFAULT
```

临时修改为：

```text
default y # KI_AUTO_HOOK_DEFAULT
```

自动 Hook 模式下不会执行 `kernel/integrate.sh`。

自动后端顺序：

```text
raw syscall tracepoint
        ↓
arm64 syscall-wrapper kretprobe
```

tracepoint 使用现代 GKI 的 `sys_enter` / `sys_exit` syscall tracepoint API；kprobe 则回退到 GKI 风格 arm64 syscall wrapper，并通过 return probe + task_work 在 syscall 即将返回用户空间时完成修改。

初版自动后端按 Android GKI 5.10/6.1 代际设计，仅处理原生 AArch64 `uname`，compat 32 位任务会被跳过。

## 手动 Hook

手动方式适合旧版/非 GKI 内核，或者目标内核没有可用的自动 Hook 符号或 tracepoint。

```bash
sh /path/to/Kernel-Informater/kernel/integrate.sh /path/to/kernel
```

脚本会在目标内核建立：

```text
include/linux/ki.h
include/linux/ki_kfunc.h
include/linux/ki_uname.h
```

然后在每个匹配的：

```c
if (override_release(name->release, sizeof(name->release)))
	return -EFAULT;
```

之后插入 KI Hook。

脚本使用 KI marker，重复执行不会重复插入；如果没有找到预期 Hook 点，则会停止而不会猜测其他位置。

完成后建议检查：

```bash
git diff -- kernel/sys.c include/linux
```

## 编译检测

编译检测工具位于：

```text
kernel/tools/
```

运行：

```bash
sh /path/to/Kernel-Informater/kernel/tools/build_check.sh
```

自动 Hook 检测：

```bash
sh /path/to/Kernel-Informater/kernel/tools/build_check.sh --auto-hook
```

该工具会临时以模块配置构建 KI，并在存在 `vmlinux` 时使用 `kernel/tools/check_symbol` 检查模块未定义符号是否能够在 `vmlinux` 中解析。

## 清理

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh --cleanup
```

清理会移除目标 `Kernel_Informater` 软链接、Kbuild/Kconfig 项、手动 include 软链接、`sys.c` 手动 Hook，并把 KI 自动 Hook 默认值恢复为 `n`。
