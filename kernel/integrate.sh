#!/bin/sh
set -eu

KI_KERNEL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

usage() {
	cat <<USAGE
Usage: $0 [--cleanup] [<kernel-tree>]

Manually integrate Kernel Informater uname/process hooks into a legacy kernel.

Options:
  --cleanup       Remove KI header links and all KI manual hook blocks.
  -h, --help      Show this help.
USAGE
}

MODE=install
KERNEL_ROOT=

while [ "$#" -gt 0 ]; do
	case "$1" in
		--cleanup) MODE=cleanup ;;
		-h|--help) usage; exit 0 ;;
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
FORK="$KERNEL_ROOT/kernel/fork.c"
EXEC="$KERNEL_ROOT/fs/exec.c"
EXIT="$KERNEL_ROOT/kernel/exit.c"

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
	remove_header "$KERNEL_ROOT/include/linux/ki_process.h"

	for f in "$SYSC" "$FORK" "$EXEC" "$EXIT"; do
		[ -f "$f" ] || continue
		python3 - "$f" <<'PY'
from pathlib import Path
import re
import sys

p = Path(sys.argv[1])
s = p.read_text()
patterns = [
    r'/\* KI: manual uname header begin \*/\n#include <linux/ki_uname\.h>\n/\* KI: manual uname header end \*/\n?',
    r'/\* KI: manual process header begin \*/\n#include <linux/ki_process\.h>\n/\* KI: manual process header end \*/\n?',
    r'\n?\/\* KI: manual uname hook begin \*\/.*?\/\* KI: manual uname hook end \*\/',
    r'\n?\/\* KI: manual process (?:fork|exec|exit) hook begin \*\/.*?\/\* KI: manual process (?:fork|exec|exit) hook end \*\/',
]
for pattern in patterns:
    s = re.sub(pattern, '', s, flags=re.S)
p.write_text(s)
PY
	done
	exit 0
fi

for f in "$SYSC" "$FORK" "$EXEC" "$EXIT"; do
	[ -f "$f" ] || {
		echo "[ERROR] required kernel source file not found: $f" >&2
		exit 1
	}
done

command -v python3 >/dev/null 2>&1 || {
	echo "[ERROR] python3 is required." >&2
	exit 1
}

link_header "$KI_KERNEL_DIR/include/ki.h" "$KERNEL_ROOT/include/linux/ki.h"
link_header "$KI_KERNEL_DIR/include/ki_kfunc.h" "$KERNEL_ROOT/include/linux/ki_kfunc.h"
link_header "$KI_KERNEL_DIR/kfunc/uname/ki_uname.h" "$KERNEL_ROOT/include/linux/ki_uname.h"
link_header "$KI_KERNEL_DIR/kfunc/process/ki_process.h" "$KERNEL_ROOT/include/linux/ki_process.h"

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

s = re.sub(r'\n?\/\* KI: manual uname hook begin \*\/.*?\/\* KI: manual uname hook end \*/', '', s, flags=re.S)
pattern = re.compile(
    r'(?P<indent>^[ \t]*)if \(override_release\(name->release, sizeof\(name->release\)\)\)\n'
    r'(?P<return>[ \t]*)return -EFAULT;', re.M)
matches = list(pattern.finditer(s))
if not matches:
    raise SystemExit("KI: no uname override_release() site found; refusing to guess")
parts = []
last = 0
for match in matches:
    parts.append(s[last:match.end()])
    indent = match.group('indent')
    parts.append("\n" + hook.replace("\n\t", "\n" + indent + "\t"))
    last = match.end()
parts.append(s[last:])
p.write_text("".join(parts))
print(f"[+] KI manual uname hook inserted at {len(matches)} site(s)")
PY

python3 - "$FORK" "$EXEC" "$EXIT" <<'PY'
from pathlib import Path
import re
import sys

fork, execf, exitf = map(Path, sys.argv[1:])

def ensure_header(p):
    s = p.read_text()
    marker = (
        "/* KI: manual process header begin */\n"
        "#include <linux/ki_process.h>\n"
        "/* KI: manual process header end */"
    )
    if "/* KI: manual process header begin */" not in s:
        includes = list(re.finditer(r'^#include <linux/[^>]+>\n', s, re.M))
        if includes:
            pos = includes[-1].end()
            s = s[:pos] + marker + "\n" + s[pos:]
        else:
            s = marker + "\n" + s
    return s

def clean_hooks(s):
    return re.sub(
        r'\n?\/\* KI: manual process (?:fork|exec|exit) hook begin \*\/.*?'
        r'\/\* KI: manual process (?:fork|exec|exit) hook end \*\/',
        '', s, flags=re.S)

s = clean_hooks(ensure_header(fork))
pattern = "trace_sched_process_fork(current, p);"
hook = (
    "/* KI: manual process fork hook begin */\n"
    "#ifdef CONFIG_KI\n"
    "\tki_process_manual_fork(p);\n"
    "#endif\n"
    "/* KI: manual process fork hook end */"
)
if pattern not in s:
    raise SystemExit("KI: no trace_sched_process_fork(current, p) site found in kernel/fork.c")
s = s.replace(pattern, pattern + "\n" + hook, 1)
fork.write_text(s)

s = clean_hooks(ensure_header(execf))
pattern = "/* execve succeeded */"
hook = (
    "/* KI: manual process exec hook begin */\n"
    "#ifdef CONFIG_KI\n"
    "\tki_process_manual_exec(current);\n"
    "#endif\n"
    "/* KI: manual process exec hook end */"
)
if pattern not in s:
    raise SystemExit("KI: no execve success site found in fs/exec.c")
s = s.replace(pattern, pattern + "\n" + hook, 1)
execf.write_text(s)

s = clean_hooks(ensure_header(exitf))
match = re.search(r'(?m)^void(?: __noreturn)? do_exit\(long code\)\n\{', s)
if not match:
    raise SystemExit("KI: no do_exit(long code) site found in kernel/exit.c")
hook = (
    match.group(0) + "\n"
    "/* KI: manual process exit hook begin */\n"
    "#ifdef CONFIG_KI\n"
    "\tki_process_manual_exit(current);\n"
    "#endif\n"
    "/* KI: manual process exit hook end */"
)
s = s[:match.start()] + hook + s[match.end():]
exitf.write_text(s)

print("[+] KI manual process hooks inserted")
PY
