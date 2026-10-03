#!/bin/sh
set -eu

# Kernel Informater setup script.
# This script follows the simple bootstrap/update/cleanup flow used by
# ReSukiSU/KernelSU, while keeping KI-specific automatic/manual hook handling.

GKI_ROOT=$(pwd)
KI_REPO_DIR="$GKI_ROOT/Kernel_Informater"
KI_REPO_URL="${KI_REPO_URL:-https://github.com/rrrrrUDE/Kernel_Informater.git}"

KI_DST="$GKI_ROOT/kernel/Kernel_Informater"
KCONFIG="$GKI_ROOT/kernel/Kconfig"
KMAKE="$GKI_ROOT/kernel/Makefile"
TRACEPOINT_MARKER="$GKI_ROOT/kernel/.ki_tracepoint_hook_default"

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
  --tracepoint-hook Enable Tracepoint Syscall Redirect hook (GKI 2.0 / 5.10+ only).
  --manual-hook    Use source-level hooks through kernel/integrate.sh.
  --cleanup        Revert KI integration and remove the cloned KI tree.
  -h, --help       Show this help.

Arguments:
  <commit-or-tag>  Checkout the specified branch, tag, or commit.

Environment:
  KI_REPO_URL      Override the Kernel Informater repository URL.

Examples:
  curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash
  curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --tracepoint-hook
  curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- --manual-hook
  curl -LSs https://github.com/rrrrrUDE/Kernel_Informater/raw/main/kernel/setup.sh | bash -s -- v0.2.2
  sh kernel/setup.sh --cleanup
USAGE
}

die() {
	echo "[ERROR] $*" >&2
	exit 1
}

remove_line() {
	file=$1
	line=$2

	[ -f "$file" ] || return 0

	tmp=$(mktemp)
	grep -Fvx "$line" "$file" > "$tmp" || true
	cat "$tmp" > "$file"
	rm -f "$tmp"
}

check_requirements() {
	[ -f "$GKI_ROOT/Makefile" ] || die "Not a Linux kernel source tree: $GKI_ROOT"
	[ -d "$GKI_ROOT/kernel" ] || die "kernel/ directory not found: $GKI_ROOT/kernel"
	[ -f "$KCONFIG" ] || die "kernel/Kconfig not found: $KCONFIG"
	[ -f "$KMAKE" ] || die "kernel/Makefile not found: $KMAKE"
	command -v git >/dev/null 2>&1 || die "git is required"
	command -v realpath >/dev/null 2>&1 || die "realpath is required"
}

repo_is_valid() {
	[ -d "$KI_REPO_DIR/.git" ] &&
	[ -f "$KI_REPO_DIR/kernel/Kconfig" ] &&
	[ -f "$KI_REPO_DIR/kernel/Makefile" ] &&
	[ -f "$KI_REPO_DIR/kernel/setup.sh" ]
}

clone_or_update_repo() {
	if ! repo_is_valid; then
		if [ -e "$KI_REPO_DIR" ]; then
			die "Path already exists and is not a valid Kernel Informater repository: $KI_REPO_DIR"
		fi

		echo "[+] Setting up Kernel Informater..."
		git clone --recursive "$KI_REPO_URL" "$KI_REPO_DIR"
		echo "[+] Repository cloned."
	else
		echo "[+] Updating Kernel Informater..."
		cd "$KI_REPO_DIR"

		# Match the lightweight update flow used by ReSukiSU while preserving
		# local user changes instead of allowing an update to fail unexpectedly.
		git stash push --include-untracked -m "Kernel Informater setup temporary stash" >/dev/null 2>&1 || true
		git fetch --all --tags --recurse-submodules
		git checkout main
		git pull --ff-only --recurse-submodules
		git submodule sync --recursive
		git submodule update --init --recursive
	fi

	cd "$KI_REPO_DIR"

	if [ -n "$REF" ]; then
		echo "[+] Checking out $REF..."
		git fetch --all --tags --recurse-submodules
		git checkout "$REF" || die "Unable to checkout $REF"
	else
		git checkout main
	fi

	git submodule sync --recursive
	git submodule update --init --recursive
}

kernel_version() {
	awk '/^VERSION[[:space:]]*=/ { version=$3 } /^PATCHLEVEL[[:space:]]*=/ { patchlevel=$3 } END { if (version != "" && patchlevel != "") print version "." patchlevel; else exit 1 }' "$GKI_ROOT/Makefile"
}

tracepoint_hook_supported() {
	version=$(kernel_version 2>/dev/null || echo 0.0)
	major=${version%%.*}
	rest=${version#*.}
	minor=${rest%%.*}

	[ "$major" -gt 5 ] || { [ "$major" -eq 5 ] && [ "$minor" -ge 10 ]; }
}

set_hook_default() {
	value=$1

	[ "$value" = y ] || [ "$value" = n ] || die "Invalid hook default: $value"

	[ -f "$KI_REPO_DIR/kernel/Kconfig" ] ||
		die "Kernel Informater Kconfig not found."

	if grep -Eq '^[[:space:]]*default [yn] # KI_TRACEPOINT_HOOK_DEFAULT[[:space:]]*$' "$KI_REPO_DIR/kernel/Kconfig"; then
		sed -i -E \
			"s/^([[:space:]]*)default [yn] # KI_TRACEPOINT_HOOK_DEFAULT[[:space:]]*$/\1default $value # KI_TRACEPOINT_HOOK_DEFAULT/" \
			"$KI_REPO_DIR/kernel/Kconfig"
	else
		die "KI_TRACEPOINT_HOOK default marker not found in $KI_REPO_DIR/kernel/Kconfig"
	fi

	printf '%s\n' "$value" > "$TRACEPOINT_MARKER"
	echo "[+] CONFIG_KI_TRACEPOINT_HOOK default set to $value"
}

ensure_tracepoint_default() {
	set_hook_default y
}

ensure_manual_default() {
	set_hook_default n
}

link_kernel_tree() {
	if [ -L "$KI_DST" ]; then
		[ "$(readlink -f "$KI_DST")" = "$(readlink -f "$KI_REPO_DIR/kernel")" ] ||
			die "Existing Kernel_Informater symlink points elsewhere: $KI_DST"
		echo "[OK] Kernel_Informater symlink already exists."
		return
	fi

	[ ! -e "$KI_DST" ] || die "Path already exists: $KI_DST"

	cd "$GKI_ROOT/kernel"
	ln -s "$(realpath --relative-to="$GKI_ROOT/kernel" "$KI_REPO_DIR/kernel")" Kernel_Informater
	echo "[+] Symlink created: $KI_DST"
}

integrate_kernel_tree() {
	# These modifications are intentionally idempotent, like the ReSukiSU
	# setup flow.
	grep -Fqx "$KCONFIG_LINE" "$KCONFIG" 2>/dev/null ||
		printf '\n%s\n' "$KCONFIG_LINE" >> "$KCONFIG"

	grep -Fqx "$KMAKE_LINE" "$KMAKE" 2>/dev/null ||
		printf '\n%s\n' "$KMAKE_LINE" >> "$KMAKE"

	echo "[+] Kernel Kconfig/Makefile integration checked."
}

run_manual_hook() {
	[ -x "$KI_REPO_DIR/kernel/integrate.sh" ] ||
		die "kernel/integrate.sh is missing or not executable."

	echo "[+] Running manual kernel integration..."
	"$KI_REPO_DIR/kernel/integrate.sh" "$GKI_ROOT"
}

remove_manual_hook() {
	[ -x "$KI_REPO_DIR/kernel/integrate.sh" ] || return 0
	"$KI_REPO_DIR/kernel/integrate.sh" --cleanup "$GKI_ROOT" || true
}

cleanup() {
	echo "[+] Cleaning up Kernel Informater..."

	# Only operate on a real KI checkout. Never remove an unrelated
	# directory that happens to use the same name.
	if repo_is_valid; then
		remove_manual_hook
	fi

	if [ -L "$KI_DST" ]; then
		rm -f "$KI_DST"
		echo "[-] Symlink removed."
	fi

	remove_line "$KCONFIG" "$KCONFIG_LINE"
	remove_line "$KMAKE" "$KMAKE_LINE"

	if [ -f "$AUTO_MARKER" ]; then
		echo "[-] Hook-default marker removed."
		rm -f "$AUTO_MARKER"
	fi

	if repo_is_valid; then
		rm -rf "$KI_REPO_DIR"
		echo "[-] Kernel Informater directory removed."
	fi

	echo "[+] Cleanup complete."
}

select_hook_mode() {
	[ "$HOOK_MODE" = prompt ] || return 0

	if [ -r /dev/tty ] && [ -w /dev/tty ]; then
		printf "Enable Tracepoint Syscall Redirect hook? [y/N]: " > /dev/tty
		read -r answer < /dev/tty || answer=

		case "$answer" in
			y|Y|yes|YES|Yes)
				HOOK_MODE=tracepoint
				;;
			*)
				HOOK_MODE=manual
				;;
		esac
	else
		# curl | bash without a controlling tty must never block.
		HOOK_MODE=manual
		echo "[!] Interactive terminal not available; using manual hook integration."
	fi
}

while [ "$#" -gt 0 ]; do
	case "$1" in
		--cleanup)
			MODE=cleanup
			;;
		--tracepoint-hook)
			HOOK_MODE=tracepoint
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
			[ -z "$REF" ] || die "Only one commit/tag/ref may be specified."
			REF=$1
			;;
	esac
	shift
done

check_requirements

if [ "$MODE" = cleanup ]; then
	cleanup
	exit 0
fi

select_hook_mode

# Refuse unsupported Tracepoint Syscall Redirect deployment before cloning,
# linking, or modifying the target kernel tree.
if [ "$HOOK_MODE" = tracepoint ]; then
	version=$(kernel_version 2>/dev/null || echo "unknown")
	major=$(printf "%s" "$version" | cut -d. -f1)
	minor=$(printf "%s" "$version" | cut -d. -f2)
	case "$major:$minor" in
		5:10|5:1[1-9]|5:[2-9][0-9]|[6-9]:*|[1-9][0-9]:*) ;;
		*) die "Tracepoint Syscall Redirect requires kernel VERSION/PATCHLEVEL >= 5.10 (detected $version). Use --manual-hook." ;;
	esac
	echo "[OK] Kernel VERSION/PATCHLEVEL check passed: $version"
fi

clone_or_update_repo
link_kernel_tree
integrate_kernel_tree

case "$HOOK_MODE" in
	tracepoint)
		version=$(kernel_version 2>/dev/null || echo "unknown")
		major=$(printf "%s" "$version" | cut -d. -f1)
		minor=$(printf "%s" "$version" | cut -d. -f2)
		case "$major:$minor" in
			5:10|5:1[1-9]|5:[2-9][0-9]|[6-9]:*|[1-9][0-9]:*) ;;
			*) die "Tracepoint Syscall Redirect requires kernel VERSION/PATCHLEVEL >= 5.10 (detected $version). Use --manual-hook." ;;
		esac
		remove_manual_hook
		ensure_tracepoint_default
		echo "[+] Automatic GKI Tracepoint Syscall Redirect hook selected."
		echo "[+] Kernel VERSION/PATCHLEVEL check passed: $version."
		;;
	manual)
		ensure_manual_default
		run_manual_hook
		;;
	*)
		die "Invalid hook mode: $HOOK_MODE"
		;;
esac

echo "[+] Kernel Informater setup complete."
echo "[!] Enable CONFIG_KI=y in the target kernel configuration."
