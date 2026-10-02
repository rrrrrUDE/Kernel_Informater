#!/bin/sh
set -eu

KI_KERNEL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

usage() {
	cat <<USAGE
Usage: $0 [--cleanup] [<kernel-tree>]

Manually integrate Kernel Informater uname hooks into kernel/sys.c.

Options:
  --cleanup       Remove KI header links and KI hook blocks.
  -h, --help      Show this help.
USAGE
}

MODE=install
KERNEL_ROOT=

while [ "$#" -gt 0 ]; do
	case "$1" in
		--cleanup)
			MODE=cleanup
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			[ -z "$KERNEL_ROOT" ] || {
				echo "[ERROR] Too many kernel tree arguments." >&2
				exit 2
			}
			KERNEL_ROOT=$1
			;;
	esac
	shift
done

KERNEL_ROOT=${KERNEL_ROOT:-$(pwd)}
KERNEL_ROOT=$(CDPATH= cd -- "$KERNEL_ROOT" && pwd)
SYSC="$KERNEL_ROOT/kernel/sys.c"

link_header() {
	src=$1
	dst=$2

	if [ -L "$dst" ]; then
		[ "$(readlink -f "$dst")" = "$(readlink -f "$src")" ] || {
			echo "[ERROR] Header link points elsewhere: $dst" >&2
			return 1
		}
		return 0
	fi

	[ ! -e "$dst" ] || {
		echo "[ERROR] Header path already exists: $dst" >&2
		return 1
	}

	mkdir -p "$(dirname "$dst")"
	ln -s "$(realpath --relative-to="$(dirname "$dst")" "$src")" "$dst"
}

remove_header() {
	[ ! -L "$1" ] || rm -f "$1"
}

if [ "$MODE" = cleanup ]; then
	remove_header "$KERNEL_ROOT/include/linux/ki.h"
	remove_header "$KERNEL_ROOT/include/linux/ki_kfunc.h"
	remove_header "$KERNEL_ROOT/include/linux/ki_uname.h"

	if [ -f "$SYSC" ]; then
		python3 - "$SYSC" <<'PY'
from pathlib import Path
import re
import sys

p = Path(sys.argv[1])
s = p.read_text()

s = re.sub(
    r'/\* KI: manual uname header begin \*/\n#include <linux/ki_uname\.h>\n/\* KI: manual uname header end \*/\n?',
    '', s)

s = re.sub(
    r'/\* KI: manual uname hook begin \*/.*?/\* KI: manual uname hook end \*/\n?',
    '', s, flags=re.S)

p.write_text(s)
PY
	fi
	exit 0
fi

[ -f "$SYSC" ] || {
	echo "[ERROR] kernel/sys.c not found: $SYSC" >&2
	exit 1
}
command -v python3 >/dev/null 2>&1 || {
	echo "[ERROR] python3 is required." >&2
	exit 1
}

link_header "$KI_KERNEL_DIR/include/ki.h" "$KERNEL_ROOT/include/linux/ki.h"
link_header "$KI_KERNEL_DIR/include/ki_kfunc.h" "$KERNEL_ROOT/include/linux/ki_kfunc.h"
link_header "$KI_KERNEL_DIR/kfunc/uname/ki_uname.h" "$KERNEL_ROOT/include/linux/ki_uname.h"

python3 - "$SYSC" <<'PY'
from pathlib import Path
import re
import sys

p = Path(sys.argv[1])
s = p.read_text()

header = (
    "/* KI: manual uname header begin */\n"
    "#include <linux/ki_uname.h>\n"
    "/* KI: manual uname header end */"
)

hook = (
    "/* KI: manual uname hook begin */\n"
    "#ifdef CONFIG_KI\n"
    "\tif (ki_uname_override_release(name->release, sizeof(name->release)))\n"
    "\t\treturn -EFAULT;\n"
    "#endif\n"
    "/* KI: manual uname hook end */"
)

if "/* KI: manual uname header begin */" not in s:
    marker = "#include <linux/utsname.h>"
    if marker in s:
        s = s.replace(marker, marker + "\n" + header, 1)
    else:
        s = header + "\n" + s

# Remove an old KI hook block before reinserting it at every native
# override_release() site. This makes repeated execution idempotent while
# still covering newuname(), uname() and olduname() when all three exist.
s = re.sub(
    r'\n?\/\* KI: manual uname hook begin \*\/.*?\/\* KI: manual uname hook end \*\/',
    '', s, flags=re.S)

pattern = re.compile(
    r'(?P<indent>^[ \t]*)if \(override_release\(name->release, sizeof\(name->release\)\)\)\n'
    r'(?P<return>[ \t]*)return -EFAULT;',
    re.M)

matches = list(pattern.finditer(s))
if not matches:
    raise SystemExit(
        "KI: no override_release(name->release, sizeof(name->release)) sites found; "
        "refusing to guess"
    )

parts = []
last = 0
for match in matches:
    parts.append(s[last:match.end()])
    indent = match.group('indent')
    hook_text = hook.replace('\n\t', '\n' + indent + '\t')
    parts.append('\n' + hook_text)
    last = match.end()
parts.append(s[last:])
s = ''.join(parts)

p.write_text(s)
print(f"[+] KI manual hook inserted at {len(matches)} override_release() site(s)")
if len(matches) < 3:
    print("[!] Fewer than three sites were found; review newuname/uname/olduname manually.")
PY
