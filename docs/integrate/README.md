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

输入 `y` 使用Kprobe Hook；输入 `n` 或者其他（包括直接回车）都会选择手动源码集成（执行 `kernel/integrate.sh`）。

也可以这样子：

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
sh /path/to/Kernel-Informater/kernel/setup.sh --manual-hook
```

如果你想使用Kprobe Hook，执行：

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
```
或者在询问阶段输入`y`

## 清理集成

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh --cleanup
```
