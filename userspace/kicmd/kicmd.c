#include "kicmd_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv)
{
    atexit(close_ki);
    if (argc < 2) { print_help(); return 0; }
    if (!strcmp(argv[1], KICMD_CMD_HELP)) return cli_result(cmd_help(argc - 1, argv + 1));
    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) { print_help(); return 0; }
    if (!strcmp(argv[1], KICMD_CMD_VERSION) || !strcmp(argv[1], "-V") || !strcmp(argv[1], "--version")) {
        if (argc > 2) return cli_unexpected_argument("kicmd version", argv[2]);
        return cli_result(print_version());
    }
    if (!strcmp(argv[1], KICMD_CMD_SAFEMODE)) return cli_result(cmd_safemode(argc - 1, argv + 1));
    if (!strcmp(argv[1], KICMD_CMD_CONFIG)) return cli_result(cmd_config(argc - 1, argv + 1));
    if (!strcmp(argv[1], KICMD_CMD_LIST)) return cli_result(cmd_list(argc - 1, argv + 1));
    if (!strcmp(argv[1], KICMD_CMD_FUNC)) return cli_result(cmd_func(argc - 1, argv + 1));
    { static const char *const commands[] = { KICMD_CMD_SAFEMODE, KICMD_CMD_CONFIG, KICMD_CMD_LIST, KICMD_CMD_FUNC, KICMD_CMD_HELP, KICMD_CMD_VERSION }; return cli_unknown_command("command", argv[1], "kicmd <COMMAND>", commands, sizeof(commands) / sizeof(commands[0])); }
}
