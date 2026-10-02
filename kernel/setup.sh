#!/bin/sh
set -eu

GKI_ROOT=$(pwd)
KI_KERNEL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
KI_ROOT=$(CDPATH= cd -- "$KI_KERNEL_DIR/.." && pwd)

usage() {
	cat <<USAGE
Usage: $0 [--cleanup | --auto-hook | --manual-hook]

Setup Kernel Informater in the current Linux kernel source tree.

Options:
  --auto-hook      Enable the GKI tracepoint/kprobe hook backend and
                   set CONFIG_KI_KPROBEHOOK default to y.
  --manual-hook    Use kernel/integrate.sh for source-level hooks.
                   This is also the default when auto hook is declined.
  --cleanup        Remove KI integration from the current kernel tree.
  -h, --help       Show this help message.

When no hook option is given, the script asks whether automatic GKI
tracepoint/kprobe hooks are wanted. Press Enter or answer N to use the
manual kernel/integrate.sh path.
USAGE
}

MODE=install
HOOK_MODE=prompt

while [ "$#" -gt 0 ]; do
	case "$1" in
		--cleanup)
			MODE=cleanup
			;;
		--auto-hook)
			HOOK_MODE=auto
			;;
		--manual-hook)
			HOOK_MODE=manual
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "[ERROR] Unknown option: $1" >&2
			usage >&2
			exit 2
			;;
	esac
	shift
done

[ -f "$GKI_ROOT/Makefile" ] || {
	echo "[ERROR] Not a Linux kernel source tree: $GKI_ROOT" >&2
	exit 1
}
[ -d "$GKI_ROOT/kernel" ] || {
	echo "[ERROR] kernel/ directory not found: $GKI_ROOT/kernel" >&2
	exit 1
}
[ -f "$GKI_ROOT/kernel/Kconfig" ] || {
	echo "[ERROR] kernel/Kconfig not found: $GKI_ROOT/kernel/Kconfig" >&2
	exit 1
}
[ -f "$GKI_ROOT/kernel/Makefile" ] || {
	echo "[ERROR] kernel/Makefile not found: $GKI_ROOT/kernel/Makefile" >&2
	exit 1
}

KI_DST="$GKI_ROOT/kernel/Kernel_Informater"
KCONFIG="$GKI_ROOT/kernel/Kconfig"
KMAKE="$GKI_ROOT/kernel/Makefile"
AUTO_MARKER="$GKI_ROOT/kernel/.ki_auto_hook_default"
KCONFIG_LINE='source "kernel/Kernel_Informater/Kconfig"'
KMAKE_LINE='obj-$(CONFIG_KI) += Kernel_Informater/'

remove_line() {
	file=$1
	line=$2
	tmp=$(mktemp)
	grep -Fvx "$line" "$file" > "$tmp" || true
	cat "$tmp" > "$file"
	rm -f "$tmp"
}

set_auto_default() {
	if [ -f "$AUTO_MARKER" ]; then
		if grep -Eq '^[[:space:]]*default y # KI_AUTO_HOOK_DEFAULT[[:space:]]*$' "$KI_KERNEL_DIR/Kconfig"; then
			echo "[OK] CONFIG_KI_KPROBEHOOK automatic default already configured"
			return
		fi
		echo "[ERROR] KI_KPROBEHOOK auto-hook marker state is inconsistent." >&2
		exit 1
	fi

	if grep -Eq '^[[:space:]]*default n # KI_AUTO_HOOK_DEFAULT[[:space:]]*$' "$KI_KERNEL_DIR/Kconfig"; then
		sed -i -E 's/^([[:space:]]*)default n # KI_AUTO_HOOK_DEFAULT[[:space:]]*$/\1default y # KI_AUTO_HOOK_DEFAULT/' "$KI_KERNEL_DIR/Kconfig"
		printf '%s\n' changed > "$AUTO_MARKER"
		echo "[+] CONFIG_KI_KPROBEHOOK default changed to y"
	elif grep -Eq '^[[:space:]]*default y # KI_AUTO_HOOK_DEFAULT[[:space:]]*$' "$KI_KERNEL_DIR/Kconfig"; then
		printf '%s\n' unchanged > "$AUTO_MARKER"
		echo "[OK] CONFIG_KI_KPROBEHOOK default is already y"
	else
		echo "[ERROR] KI_KPROBEHOOK default marker not found in $KI_KERNEL_DIR/Kconfig" >&2
		exit 1
	fi
}

clear_auto_default() {
	if [ -f "$AUTO_MARKER" ] && grep -Fqx changed "$AUTO_MARKER"; then
		sed -i -E 's/^([[:space:]]*)default y # KI_AUTO_HOOK_DEFAULT[[:space:]]*$/\1default n # KI_AUTO_HOOK_DEFAULT/' "$KI_KERNEL_DIR/Kconfig"
		echo "[-] CONFIG_KI_KPROBEHOOK default restored to n"
	fi
	rm -f "$AUTO_MARKER"
}

ensure_repo_uapi_link() {
	local dst="$KI_ROOT/kernel/uapi"
	local src="../uapi"

	if [ -L "$dst" ]; then
		if [ "$(readlink -f "$dst")" != "$(readlink -f "$KI_ROOT/uapi")" ]; then
			echo "[ERROR] Repository kernel/uapi symlink points elsewhere:" >&2
			readlink "$dst" >&2 || true
			exit 1
		fi
		return
	fi

	if [ -e "$dst" ]; then
		echo "[ERROR] Repository path already exists: $dst" >&2
		exit 1
	fi

	[ -d "$KI_ROOT/uapi" ] || {
		echo "[ERROR] Repository UAPI directory not found: $KI_ROOT/uapi" >&2
		exit 1
	}

	ln -s "$src" "$dst"
	echo "[+] UAPI symlink created: $dst -> $src"
}

link_kernel_tree() {
	cd "$KI_ROOT/kernel"

	if [ -L "$KI_DST" ]; then
		if [ "$(readlink -f "$KI_DST")" != "$(readlink -f "$KI_ROOT/kernel")" ]; then
			echo "[ERROR] Existing Kernel_Informater symlink points elsewhere:" >&2
			readlink "$KI_DST" >&2 || true
			exit 1
		fi
		echo "[OK] $KI_DST -> $KI_ROOT/kernel"
		return
	fi

	if [ -e "$KI_DST" ]; then
		echo "[ERROR] Path already exists: $KI_DST" >&2
		exit 1
	fi

	ln -s "$(realpath --relative-to="$(dirname "$KI_DST")" "$KI_ROOT/kernel")" "$KI_DST"
	echo "[+] Symlink created: $KI_DST -> $KI_ROOT/kernel"
}

cleanup() {
	echo "[+] Cleaning up Kernel Informater from: $GKI_ROOT"

	"$KI_KERNEL_DIR/integrate.sh" --cleanup "$GKI_ROOT" || true

	if [ -L "$KI_DST" ]; then
		rm -f "$KI_DST"
		echo "[-] Kernel_Informater symlink removed."
	fi

	remove_line "$KCONFIG" "$KCONFIG_LINE"
	remove_line "$KMAKE" "$KMAKE_LINE"
	clear_auto_default

	echo "[+] Cleanup complete."
}

if [ "$MODE" = cleanup ]; then
	cleanup
	exit 0
fi

if [ "$HOOK_MODE" = prompt ]; then
	if [ -t 0 ]; then
		printf "Enable automatic GKI tracepoint/kprobe hook? [y/N]: "
		read -r answer || answer=
		case "$answer" in
			y|Y|yes|YES|Yes)
				HOOK_MODE=auto
				;;
			*)
				HOOK_MODE=manual
				;;
			esac
	else
		HOOK_MODE=manual
		echo "[!] Non-interactive setup: using manual hook integration."
	fi
fi

ensure_repo_uapi_link
link_kernel_tree

cd "$GKI_ROOT"
grep -Fqx "$KCONFIG_LINE" "$KCONFIG" || {
	printf '\n%s\n' "$KCONFIG_LINE" >> "$KCONFIG"
	echo "[+] Modified kernel/Kconfig"
}
grep -Fqx "$KMAKE_LINE" "$KMAKE" || {
	printf '\n%s\n' "$KMAKE_LINE" >> "$KMAKE"
	echo "[+] Modified kernel/Makefile"
}

case "$HOOK_MODE" in
	auto)
		set_auto_default
		echo "[+] Automatic GKI hook selected; kernel/integrate.sh will not run."
		;;
	manual)
		clear_auto_default
		echo "[+] Manual hook selected; running kernel/integrate.sh."
		"$KI_KERNEL_DIR/integrate.sh" "$GKI_ROOT"
		;;
esac

echo "[+] Kernel Informater setup complete."
echo "[!] Enable CONFIG_KI=y in the target kernel configuration."
