#include "../../include/kicmd_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <unistd.h>

int cmd_safemode(int argc, char **argv)
{
	int ret;

	if (argc < 2 || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
		fputs(kicmd_help_safemode, stdout);
		return argc < 2 ? 1 : 0;
	}

	if (require_ki_driver() < 0)
		return 1;

	ret = ensure_userd_dir();
	if (ret)
		return 1;

	if (!strcmp(argv[1], KICMD_SUB_ENABLE)) {
		int fd;
		if (argc > 2)
			return cli_unexpected_argument("kicmd safemode enable", argv[2]);
		fd = open(KI_USER_SAFE_MODE, O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
		if (fd < 0) {
			fprintf(stderr, "Error: create %s: %s\n",
				KI_USER_SAFE_MODE, strerror(errno));
			return 1;
		}
		close(fd);
		debug_log("safemode enable");
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_DISABLE)) {
		if (argc > 2)
			return cli_unexpected_argument("kicmd safemode disable", argv[2]);
		if (unlink(KI_USER_SAFE_MODE) && errno != ENOENT) {
			fprintf(stderr, "Error: remove %s: %s\n",
				KI_USER_SAFE_MODE, strerror(errno));
			return 1;
		}
		debug_log("safemode disable");
		return 0;
	}

	{ static const char *const commands[] = { KICMD_SUB_ENABLE, KICMD_SUB_DISABLE, KICMD_CMD_HELP }; return cli_unknown_command("subcommand", argv[1], "kicmd safemode <COMMAND>", commands, sizeof(commands) / sizeof(commands[0])); }
}
