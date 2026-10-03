# Kernel Informater Kernel Integration

## Initialization

Run the following command from the root directory of the target kernel source tree:

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash
```

If no Hook option is specified, the setup script prompts:

```text
Enable Tracepoint Syscall Redirect hook? [y/N]:
```

When you enter `y`, the setup script reads `VERSION` and `PATCHLEVEL` directly from the target kernel root `Makefile`. Automatic Tracepoint Syscall Redirect deployment is allowed only for kernel 5.10 or newer. If the detected version is below 5.10, setup stops before cloning or modifying the KI integration and asks you to use Manual Hook integration.

Enter `n`, any other value, or press Enter to use Manual Hook integration.

### Manual Hook

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --manual-hook
```

### Tracepoint Syscall Redirect Hook

For an Android GKI 2.0 / 5.10+ kernel, use:

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --tracepoint-hook
```

The `--tracepoint-hook` option performs the same `VERSION`/`PATCHLEVEL` check before any KI repository clone, symlink creation, or target-tree modification.

## Cleanup

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --cleanup
```
