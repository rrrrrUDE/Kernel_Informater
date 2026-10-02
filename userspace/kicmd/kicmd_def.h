#ifndef KICMD_DEF_H
#define KICMD_DEF_H

#include <limits.h>

#include "ki_uapi.h"

#define KICMD_NAME "kicmd"
#define KICMD_VERSION KI_VERSION_STRING

/* All KI userspace state intentionally lives below /data/. */
#define KICMD_USERD_DIR        KI_USERD_DIR
#define KI_USERD_CONFIG        KI_USERD_CONFIG_PATH
#define KI_USERD_SAFE_MODE     KI_USERD_SAFE_MODE_PATH
#define KI_USERD_DEBUG_LOG     KI_USERD_DEBUG_LOG_PATH

#define KICMD_CONFIG_LINE_MAX  512
#define KICMD_CONFIG_TMP       KICMD_USERD_DIR "/.config.tmp.XXXXXX"

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

#define KICMD_UNAME_KEY_COUNT 6

static const char *const kicmd_uname_keys[KICMD_UNAME_KEY_COUNT] = {
	"sysname",
	"nodename",
	"release",
	"version",
	"machine",
	"domainname",
};

static const char kicmd_help[] =
	"Kernel Informater userspace cli\n"
	"\n"
	"Usage: kicmd <COMMAND>\n"
	"\n"
	"Commands:\n"
	"  safemode        Manage Kernel Informater safe mode\n"
	"  config          Manage persistent Kernel Informater configurations\n"
	"  list            Show real kernel information\n"
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
	"  del <kfunc>                  Delete persistent configuration\n"
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
	"Usage: kicmd list [<kfunc>]\n"
	"\n"
	"Show real kernel information before Kernel Informater modifications.\n"
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
	"  help                         Print help\n"
	"\n"
	"Options:\n"
	"  -h, --help                   Print help\n";

#endif /* KICMD_DEF_H */
