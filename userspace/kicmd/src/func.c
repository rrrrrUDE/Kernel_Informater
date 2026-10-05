#include "../kicmd_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <unistd.h>

static int ioctl_value(unsigned long request,
		       const char *kfunc, const char *key, const char *value)
{
	struct ki_ioc_value v;
	int fd;
memset(&v, 0, sizeof(v));
	strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	strncpy(v.key, key, sizeof(v.key) - 1);
	if (value)
		strncpy(v.value, value, sizeof(v.value) - 1);

	fd = open_ki_checked();
	if (fd < 0)
		return 1;
	{
		int feature_ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_FUNC);
		if (feature_ret) {
			fprintf(stderr, "Error: kfunc '%s' does not support func: %s\n", kfunc, strerror(-feature_ret));
			close(fd);
			return 1;
		}
	}
	if (ki_ioctl( request, &v) < 0) {
		fprintf(stderr, "Error: ioctl: %s\n", strerror(errno));
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}

static int ioctl_key(unsigned long request, const char *kfunc, const char *key)
{
	struct ki_ioc_key v;
	int fd;
memset(&v, 0, sizeof(v));
	strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	if (key)
		strncpy(v.key, key, sizeof(v.key) - 1);
	fd = open_ki_checked();
	if (fd < 0)
		return 1;
	{
		int feature_ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_FUNC);
		if (feature_ret) {
			fprintf(stderr, "Error: kfunc '%s' does not support func: %s\n", kfunc, strerror(-feature_ret));
			close_ki();
			return 1;
		}
	}
	if (ki_ioctl( request, &v) < 0) {
		fprintf(stderr, "Error: ioctl: %s\n", strerror(errno));
		close_ki();
		return 1;
	}
	close_ki();
	return 0;
}

static int ioctl_kfunc(unsigned long request, const char *kfunc)
{
	struct ki_ioc_kfunc v;
	int fd;
memset(&v, 0, sizeof(v));
	if (kfunc)
		strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	fd = open_ki_checked();
	if (fd < 0)
		return 1;
	if (kfunc && *kfunc) {
		int feature_ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_FUNC);
		if (feature_ret) {
			fprintf(stderr, "Error: kfunc '%s' does not support func: %s\n", kfunc, strerror(-feature_ret));
			close_ki();
			return 1;
		}
	}
	if (ki_ioctl( request, &v) < 0) {
		fprintf(stderr, "Error: ioctl: %s\n", strerror(errno));
		close_ki();
		return 1;
	}
	close_ki();
	return 0;
}

static int cmd_func(int argc, char **argv)
{
	int ret;

	if (argc < 2 || !strcmp(argv[1], "-h") ||
		!strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
		fputs(kicmd_help_func, stdout);
		return argc < 2 ? 1 : 0;
	}

	/*
	 * module operations are native module syscalls and do not depend on
	 * Kernel Informater, so they intentionally bypass the driver check.
	 */
	if (strcmp(argv[1], "module") && require_ki_driver() < 0)
		return 1;

	if (!strcmp(argv[1], "process"))
		return cmd_func_process(argc - 1, argv + 1);
	if (!strcmp(argv[1], "module"))
		return module_func(argc - 1, argv + 1);
	if (!strcmp(argv[1], "filesystem"))
		return filesystem_func(argc - 1, argv + 1);

	if (!strcmp(argv[1], KICMD_SUB_SET)) {
		if (argc < 5)
			return cli_missing_argument("kicmd func set <kfunc> <key> <value>",
				argc < 3 ? "kfunc" : argc < 4 ? "key" : "value");
		if (argc > 5)
			return cli_unexpected_argument("kicmd func set <kfunc> <key> <value>", argv[5]);
		ret = ioctl_value(KI_IOC_FUNC_VALUE_SET, argv[2], argv[3], argv[4]);
		debug_log("func set %s.%s=%s", argv[2], argv[3], argv[4]);
		return ret;
	}

	if (!strcmp(argv[1], KICMD_SUB_UNSET)) {
		if (argc < 4)
			return cli_missing_argument("kicmd func unset <kfunc> <key>",
				argc < 3 ? "kfunc" : "key");
		if (argc > 4)
			return cli_unexpected_argument("kicmd func unset <kfunc> <key>", argv[4]);
		ret = ioctl_key(KI_IOC_FUNC_VALUE_UNSET, argv[2], argv[3]);
		debug_log("func unset %s.%s", argv[2], argv[3]);
		return ret;
	}

	if (!strcmp(argv[1], KICMD_SUB_RESET)) {
		if (argc > 3)
			return cli_unexpected_argument("kicmd func reset [<kfunc>]", argv[3]);
		ret = ioctl_kfunc(KI_IOC_FUNC_KFUNC_RESET, argc == 3 ? argv[2] : "");
		debug_log("func reset %s", argc == 3 ? argv[2] : "all");
		return ret;
	}

	{ static const char *const commands[] = { KICMD_SUB_SET, KICMD_SUB_UNSET, KICMD_SUB_RESET, "process", "module", "filesystem", KICMD_CMD_HELP }; return cli_unknown_command("subcommand", argv[1], "kicmd func <COMMAND>", commands, sizeof(commands) / sizeof(commands[0])); }
}