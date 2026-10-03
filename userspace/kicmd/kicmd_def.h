#ifndef KICMD_DEF_H
#define KICMD_DEF_H

#include <limits.h>

#include "../../uapi/ki_uapi.h"

#define KICMD_NAME "kicmd"

#define KICMD_VERSION_MAJOR 1
#define KICMD_VERSION_MINOR 0
#define KICMD_VERSION_PATCH 0
#define KICMD_VERSION_STRING "1.0.0"

/* All KI userspace state intentionally lives below /data/adb/ki_user. */
#define KICMD_USER_DIR         KI_USER_DIR
#define KI_USER_CONFIG        KI_USER_CONFIG_PATH
#define KI_USER_SAFE_MODE     KI_USER_SAFE_MODE_PATH
#define KI_USER_DEBUG_LOG     KI_USER_DEBUG_LOG_PATH

#define KICMD_CONFIG_LINE_MAX  512
#define KICMD_CONFIG_TMP       KICMD_USER_DIR "/.config.tmp.XXXXXX"

#define KICMD_CMD_SAFEMODE "safemode"
#define KICMD_CMD_CONFIG   "config"
#define KICMD_CMD_LIST     "list"
#define KICMD_CMD_FUNC     "func"
#define KICMD_CMD_HELP     "help"
#define KICMD_CMD_VERSION  "version"

#define KICMD_SUB_ENABLE    "enable"
#define KICMD_SUB_DISABLE   "disable"
#define KICMD_SUB_SET       "set"
#define KICMD_SUB_UNSET     "unset"
#define KICMD_SUB_DEL       "del"
#define KICMD_SUB_RESET     "reset"
#define KICMD_SUB_ACTIVE    "active"
#define KICMD_SUB_INACTIVE  "inactive"
#define KICMD_SUB_LIST      "list"

static const char kicmd_help[] =
	"Kernel Informater userspace cli\n"
	"\n"
	"Usage: kicmd <COMMAND>\n"
	"\n"
	"Commands:\n"
	"  safemode        Manage Kernel Informater safe mode\n"
	"  config          Manage persistent Kernel Informater configurations\n"
	"  list            Show real kernel information and process information\n"
	"  func            Manage temporary runtime kernel information\n"
	"  help            Print this message or the help of the given subcommand(s)\n"
	"  version         Print version\n"
	"\n"
	"Options:\n"
	"  -h, --help      Print help\n"
	"  -V, --version   Print version\n";

static const char kicmd_help_safemode[] =
	"Usage: kicmd safemode <COMMAND>\n"
	"\n"
	"Commands:\n"
	"  enable       Enable safe mode\n"
	"  disable      Disable safe mode\n"
	"  help         Print this message\n"
	"\n"
	"Options:\n"
	"  -h, --help   Print help\n";

static const char kicmd_help_config[] =
	"Usage: kicmd config <COMMAND>\n"
	"\n"
	"Commands:\n"
	"  del <kfunc> [key]            Delete one key, or all configuration when key is omitted\n"
	"  set <kfunc> <key> <value>    Set persistent configuration\n"
	"  unset <kfunc> <key>          Remove persistent configuration\n"
	"  reset [<kfunc>]              Reset persistent configuration\n"
	"  active                       Activate persistent configuration\n"
	"  inactive                     Deactivate persistent configuration\n"
	"  list [<kfunc>]               Show persistent configuration\n"
	"  help                         Print this message\n"
	"\n"
	"Options:\n"
	"  -h, --help                   Print help\n";

static const char kicmd_help_list[] =
	"Usage: kicmd list [<kfunc>] [<pid>]\n"
	"\n"
	"Show real kernel information or process information before Kernel Informater modifications.\n"
	"\n"
	"Process:\n"
	"  list process             List visible processes\n"
	"  list process <pid>       Show process information\n"
	"  list module              List loaded kernel modules\n"
	"  list filesystem          List current filesystem mount entries\n"
	"\n"
	"Options:\n"
	"  -h, --help   Print help\n";

static const char kicmd_help_func[] =
	"Usage: kicmd func <COMMAND>\n"
	"\n"
	"Commands:\n"
	"  set <kfunc> <key> <value>    Set temporary runtime value\n"
	"  unset <kfunc> <key>          Remove temporary runtime value\n"
	"  reset [<kfunc>]              Reset temporary runtime values\n"
	"  process info <pid>           Show process information\n"
	"  process read_memory <pid> <address> <size>  Read process memory\n"
	"  process kill <pid>           Kill one process\n"
	"  process kill_tree <pid>      Kill a process and its descendants\n"
	"  module insmod <path> [args...]  Load a kernel module\n"
	"  module rmmod <name>           Remove a kernel module\n"
	"  mount add <source> <target>    Add a bind mount\n"
	"  mount umount <target>          Unmount a mount point\n"
	"  mount hot_unmount <target>     Lazy-unmount a mount point\n"
	"  help                         Print help\n"
	"\n"
	"Options:\n"
	"  -h, --help                   Print help\n";

#endif /* KICMD_DEF_H */
