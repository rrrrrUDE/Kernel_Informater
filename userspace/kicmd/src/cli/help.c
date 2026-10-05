#include "../../include/kicmd_internal.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

int print_version(void)
{
	struct ki_ioc_version version;
	int fd;

	fd = open_ki_checked();
	if (fd < 0)
		return 1;

	memset(&version, 0, sizeof(version));
	if (ki_ioctl( KI_IOC_GET_VERSION, &version) < 0) {
		fprintf(stderr, "Error: get kernel version: %s\n", strerror(errno));
		close_ki();
		return 1;
	}

	close_ki();
	printf("kernel:%u.%u.%u\n",
	       version.major, version.minor, version.patch);
	printf("userspace:%s\n", KICMD_VERSION_STRING);
	return 0;
}

void print_help(void)
{
	fputs(kicmd_help, stdout);
}

int cmd_help(int argc, char **argv)
{
	if (argc < 2) {
		print_help();
		return 0;
	}
	if (!strcmp(argv[1], KICMD_CMD_SAFEMODE)) fputs(kicmd_help_safemode, stdout);
	else if (!strcmp(argv[1], KICMD_CMD_CONFIG)) fputs(kicmd_help_config, stdout);
	else if (!strcmp(argv[1], KICMD_CMD_LIST)) fputs(kicmd_help_list, stdout);
	else if (!strcmp(argv[1], KICMD_CMD_FUNC)) fputs(kicmd_help_func, stdout);
	else {
		static const char *const commands[] = {
			KICMD_CMD_SAFEMODE, KICMD_CMD_CONFIG, KICMD_CMD_LIST,
			KICMD_CMD_FUNC, KICMD_CMD_VERSION, KICMD_CMD_HELP
		};
		return cli_unknown_command("command", argv[1], "kicmd help <COMMAND>",
			commands, sizeof(commands) / sizeof(commands[0]));
	}
	return 0;
}
