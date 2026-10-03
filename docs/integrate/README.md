# Kernel Informater 内核集成

## 初始化

在目标内核源码根目录执行：

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash
```

不指定 Hook 参数时，脚本会询问：

```text
Enable Tracepoint Syscall Redirect hook? [y/N]:
```

输入 `y` 时，Setup 会直接读取目标内核根目录 `Makefile` 中的 `VERSION` 和 `PATCHLEVEL`。只有版本为 5.10 或更高时才部署 Tracepoint Syscall Redirect Hook；低于 5.10 会直接报错并要求使用手动钩子。输入 `n` 或者其他（包括直接回车）会选择手动集成方式。

也可以这样子：

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --manual-hook
```

如果你确认目标内核为 GKI 2.0 / 5.10+，想直接使用 Tracepoint Syscall Redirect Hook，执行：

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --tracepoint-hook
```
或者在询问阶段输入`y`

## 清理集成

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --cleanup
```
