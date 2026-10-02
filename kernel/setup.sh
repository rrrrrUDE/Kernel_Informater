#!/bin/sh
set -eu

GKI_ROOT=$(pwd)
KI_REPO_DIR="$GKI_ROOT/Kernel_Informater"
KI_REPO_URL="${KI_REPO_URL:-https://github.com/rrrrrUDE/Kernel_Informater.git}"
KI_DST="$GKI_ROOT/kernel/Kernel_Informater"
KCONFIG="$GKI_ROOT/kernel/Kconfig"
KMAKE="$GKI_ROOT/kernel/Makefile"
AUTO_MARKER="$GKI_ROOT/kernel/.ki_auto_hook_default"
KCONFIG_LINE='source "kernel/Kernel_Informater/Kconfig"'
KMAKE_LINE='obj-$(CONFIG_KI) += Kernel_Informater/'

MODE=install
HOOK_MODE=prompt
REF=

usage() {
	cat <<USAGE
Kernel Informater setup

Usage: $0 [OPTIONS] [<commit-or-tag>]

Options:
  --auto-hook      Enable automatic GKI tracepoint/kprobe hooks.
  --manual-hook    Use kernel/integrate.sh for source-level hooks.
  --cleanup        Remove the Kernel Informater integration.
  -h, --help       Show this help.

Arguments:
  <commit-or-tag>  Checkout the specified KI branch, tag, or commit.

Examples:
  curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash
  curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --auto-hook
  curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --manual-hook
  curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- v0.2.2
  sh kernel/setup.sh --cleanup
USAGE
}

remove_line() {
	file=$1
	line=$2
	tmp=$(mktemp)
	grep -Fvx "$line" "$file" > "$tmp" || true
	cat "$tmp" > "$file"
	rm -f "$tmp"
}

check_kernel_tree() {
	[ -f "$GKI_ROOT/Makefile" ] || {
		echo "[ERROR] Not a Linux kernel source tree: $GKI_ROOT" >&2
		exit 1
	}
	[ -d "$GKI_ROOT/kernel" ] || {
		echo "[ERROR] kernel/ directory not found: $GKI_ROOT/kernel" >&2
		exit 1
	}
	[ -f "$KCONFIG" ] || {
		echo "[ERROR] kernel/Kconfig not found." >&2
		exit 1
	}
	[ -f "$KMAKE" ] || {
		echo "[ERROR] kernel/Makefile not found." >&2
		exit 1
	}
	command -v git >/dev/null 2>&1 || {
		echo "[ERROR] git is required." >&2
		exit 1
	}
}

clone_or_update_repo() {
	if [ ! -d "$KI_REPO_DIR/.git" ]; then
		if [ -e "$KI_REPO_DIR" ]; then
			echo "[ERROR] Path already exists and is not a KI git repository: $KI_REPO_DIR" >&2
			exit 1
		fi

		echo "[+] Cloning Kernel Informater..."
		git clone --recursive "$KI_REPO_URL" "$KI_REPO_DIR"
		echo "[+] Repository cloned."
	else
		echo "[+] Updating Kernel Informater..."
		cd "$KI_REPO_DIR"
		git stash --include-untracked || true
		git pull --recurse-submodules
		git submodule sync --recursive
		git submodule update --init --recursive
	fi

	cd "$KI_REPO_DIR"

	if [ -n "$REF" ]; then
		echo "[+] Checking out $REF..."
		git fetch --all --tags --recurse-submodules
		git checkout "$REF"
		git submodule update --init --recursive
	else
		git checkout main
		git submodule update --init --recursive
	fi
}

set_auto_default() {
	if [ -f "$AUTO_MARKER" ]; then
		echo "[OK] CONFIG_KI_KPROBEHOOK automatic default already configured"
		return
	fi

	if grep -Eq '^[[:space:]]*default n # KI_AUTO_HOOK_DEFAULT[[:space:]]*$' "$KI_REPO_DIR/kernel/Kconfig"; then
		sed -i -E 's/^([[:space:]]*)default n # KI_AUTO_HOOK_DEFAULT[[:space:]]*$/\1default y # KI_AUTO_HOOK_DEFAULT/' "$KI_REPO_DIR/kernel/Kconfig"
		printf '%s\n' changed > "$AUTO_MARKER"
		echo "[+] CONFIG_KI_KPROBEHOOK default changed to y"
	elif grep -Eq '^[[:space:]]*default y # KI_AUTO_HOOK_DEFAULT[[:space:]]*$' "$KI_REPO_DIR/kernel/Kconfig"; then
		printf '%s\n' unchanged > "$AUTO_MARKER"
		echo "[OK] CONFIG_KI_KPROBEHOOK default is already y"
	else
		echo "[ERROR] KI_KPROBEHOOK default marker not found." >&2
		exit 1
	fi
}

clear_auto_default() {
	if [ -f "$AUTO_MARKER" ] && grep -Fqx changed "$AUTO_MARKER"; then
		sed -i -E 's/^([[:space:]]*)default y # KI_AUTO_HOOK_DEFAULT[[:space:]]*$/\1default n # KI_AUTO_HOOK_DEFAULT/' "$KI_REPO_DIR/kernel/Kconfig"
		echo "[-] CONFIG_KI_KPROBEHOOK default restored to n"
	fi
	rm -f "$AUTO_MARKER"
}

link_kernel_tree() {
	if [ -L "$KI_DST" ]; then
		if [ "$(readlink -f "$KI_DST")" != "$(readlink -f "$KI_REPO_DIR/kernel")" ]; then
			echo "[ERROR] Existing Kernel_Informater symlink points elsewhere." >&2
			exit 1
		fi
		echo "[OK] $KI_DST -> $KI_REPO_DIR/kernel"
		return
	fi

	if [ -e "$KI_DST" ]; then
		echo "[ERROR] Path already exists: $KI_DST" >&2
		exit 1
	fi

	cd "$GKI_ROOT/kernel"
	ln -s "$(realpath --relative-to="$GKI_ROOT/kernel" "$KI_REPO_DIR/kernel")" "Kernel_Informater"
	echo "[+] Symlink created: $KI_DST -> $KI_REPO_DIR/kernel"
}

integrate_kernel_tree() {
	cd "$KI_REPO_DIR/kernel"

	grep -Fqx "$KCONFIG_LINE" "$KCONFIG" 2>/dev/null || {
		printf '\n%s\n' "$KCONFIG_LINE" >> "$KCONFIG"
		echo "[+] Modified kernel/Kconfig"
	}
	grep -Fqx "$KMAKE_LINE" "$KMAKE" 2>/dev/null || {
		printf '\n%s\n' "$KMAKE_LINE" >> "$KMAKE"
		echo "[+] Modified kernel/Makefile"
	}
}

run_manual_hook() {
	echo "[+] Running manual kernel integration..."
	"$KI_REPO_DIR/kernel/integrate.sh" "$GKI_ROOT"
}

cleanup() {
	echo "[+] Cleaning up Kernel Informater from: $GKI_ROOT"

	if [ -x "$KI_REPO_DIR/kernel/integrate.sh" ]; then
		"$KI_REPO_DIR/kernel/integrate.sh" --cleanup "$GKI_ROOT" || true
	fi

	if [ -L "$KI_DST" ]; then
		rm -f "$KI_DST"
		echo "[-] Kernel_Informater symlink removed."
	fi

	remove_line "$KCONFIG" "$KCONFIG_LINE"
	remove_line "$KMAKE" "$KMAKE_LINE"
	clear_auto_default

	if [ -d "$KI_REPO_DIR" ]; then
		rm -rf "$KI_REPO_DIR"
		echo "[-] Kernel Informater repository removed."
	fi

	echo "[+] Cleanup complete."
}

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
		-*)
			echo "[ERROR] Unknown option: $1" >&2
			usage >&2
			exit 2
			;;
		*)
			[ -z "$REF" ] || {
				echo "[ERROR] Only one commit/tag/ref may be specified." >&2
				exit 2
			}
			REF=$1
			;;
	esac
	shift
done

check_kernel_tree

if [ "$MODE" = cleanup ]; then
	cleanup
	exit 0
fi

clone_or_update_repo

if [ "$HOOK_MODE" = prompt ]; then
	if [ -r /dev/tty ] && [ -w /dev/tty ]; then
		printf "Enable automatic GKI tracepoint/kprobe hook? [y/N]: " > /dev/tty
		read -r answer < /dev/tty || answer=
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
		echo "[!] Interactive terminal not available: using manual hook integration."
	fi
fi

link_kernel_tree
integrate_kernel_tree

case "$HOOK_MODE" in
	auto)
		set_auto_default
		echo "[+] Automatic GKI hook selected; kernel/integrate.sh will not run."
		;;
	manual)
		clear_auto_default
		run_manual_hook
		;;
esac

echo "[+] Kernel Informater setup complete."
echo "[!] Enable CONFIG_KI=y in the target kernel configuration."
