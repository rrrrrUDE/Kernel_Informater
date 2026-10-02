# Kernel Informater Kernel Integration

## Initialization

Execute the following command in the root directory of the target kernel source:

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh
```

If no Hook parameter is specified, the script will prompt:

```text
Enable automatic GKI tracepoint/kprobe hook? [y/N]:
```

Enter `y` to use Kprobe Hook; entering `n` or anything else (including pressing Enter directly) will select manual source integration (execute `kernel/integrate.sh`).

You can also use:

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
sh /path/to/Kernel-Informater/kernel/setup.sh --manual-hook
```

If you want to use Kprobe Hook, execute:

```bash
sh /path/to/Kernel-Informater/kernel/setup.sh --auto-hook
```

Or enter `y` when prompted.

## Clean Up Integration

```bash
cd /path/to/kernel
sh /path/to/Kernel-Informater/kernel/setup.sh --cleanup
```