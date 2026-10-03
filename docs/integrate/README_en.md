# Kernel Informater Kernel Integration

## Initialization

Execute the following command in the root directory of the target kernel source:

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash
```

If no Hook parameter is specified, the script will prompt:

```text
Enable Tracepoint Syscall Redirect hook? [y/N]:
```

Enter `y` to use Tracepoint Syscall Redirect Hook on Android GKI 2.0+ (5.10+); on 5.4 and older, or GKI 1.0/non-GKI trees, use manual source integration (execute `kernel/integrate.sh`).

You can also use:

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --manual-hook
```

If you want to use Tracepoint Syscall Redirect Hook, execute:

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --tracepoint-hook
```

Or enter `y` when prompted.

## Clean Up Integration

```bash
curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --cleanup
```