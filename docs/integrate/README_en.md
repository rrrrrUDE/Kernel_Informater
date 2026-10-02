# Kernel Informater Kernel Integration

## Initialization

Execute the following command in the root directory of the target kernel source:

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash
```

If no Hook parameter is specified, the script will prompt:

```text
Enable automatic GKI tracepoint/kprobe hook? [y/N]:
```

Enter `y` to use Kprobe Hook; entering `n` or anything else (including pressing Enter directly) will select manual source integration (execute `kernel/integrate.sh`).

You can also use:

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --manual-hook
```

If you want to use Kprobe Hook, execute:

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --auto-hook
```

Or enter `y` when prompted.

## Clean Up Integration

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --cleanup
```