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

int cfg_sync(void)
{
	int ret;

	if (open_ki_checked() < 0)
		return 1;

	ret = ki_ioctl( KI_IOC_CONFIG_SYNC, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error: config sync: %s\n", strerror(errno));
		close_ki();
		return 1;
	}

	close_ki();
	return 0;
}

int cfg_set_active_and_ioctl(bool active)
{
	int fd;
	int ret;

	fd = open_ki_checked();
	if (fd < 0)
		return 1;

	if (!active) {
		ret = cfg_set_active(false);
		if (ret) {
			fprintf(stderr, "Error: save config: %s\n", strerror(-ret));
			close_ki();
			return 1;
		}
		ret = ki_ioctl( KI_IOC_CONFIG_OFF, NULL);
		if (ret < 0) {
			int saved_errno = errno;
			cfg_set_active(true);
			fprintf(stderr, "Error: config inactive: %s\n", strerror(saved_errno));
			close_ki();
			return 1;
		}
		close_ki();
		printf("Kernel Informater: persistent configuration deactivated and reset\n");
		return 0;
	}

	ret = cfg_set_active(true);
	if (ret) {
		fprintf(stderr, "Error: save config: %s\n", strerror(-ret));
		close_ki();
		return 1;
	}

	ret = ki_ioctl( KI_IOC_CONFIG_ON, NULL);
	if (ret < 0) {
		int saved_errno = errno;
		cfg_set_active(false);
		fprintf(stderr, "Error: config active: %s\n", strerror(saved_errno));
		close_ki();
		return 1;
	}

	close_ki();
	if (ret > 0) {
		printf("Kernel Informater: persistent configuration already active; no operation performed\n");
		return 0;
	}
	printf("Kernel Informater: persistent configuration activated\n");
	return 0;
}

int cfg_set_active(bool active)
{
	return write_config_with_transform("active", active ? "1" : "0",
					  NULL, NULL, false, true);
}

int cfg_set(const char *kfunc, const char *key, const char *value)
{
	char compound[KI_UAPI_KFUNC_MAX + KI_UAPI_KEY_MAX + 2];

	if (!valid_token(kfunc) || !valid_token(key) || !valid_value(value))
		return -EINVAL;
	if (snprintf(compound, sizeof(compound), "%s.%s", kfunc, key) >= (int)sizeof(compound))
		return -ENAMETOOLONG;
	return write_config_with_transform(compound, value, NULL, NULL, false, true);
}

int cfg_unset(const char *kfunc, const char *key)
{
	char compound[KI_UAPI_KFUNC_MAX + KI_UAPI_KEY_MAX + 2];

	if (!valid_token(kfunc) || !valid_token(key))
		return -EINVAL;
	if (snprintf(compound, sizeof(compound), "%s.%s", kfunc, key) >= (int)sizeof(compound))
		return -ENAMETOOLONG;
	return write_config_with_transform(NULL, NULL, NULL, compound, false, false);
}

int cfg_reset_kfunc(const char *kfunc)
{
	char prefix[KI_UAPI_KFUNC_MAX + 2];

	if (!valid_token(kfunc))
		return -EINVAL;
	snprintf(prefix, sizeof(prefix), "%s.", kfunc);
	return write_config_with_transform(NULL, NULL, prefix, NULL, false, false);
}

int cfg_reset_all(void)
{
	return write_config_with_transform("active", "0", NULL, NULL, true, true);
}

void cfg_list(const char *kfunc)
{
	FILE *fp;
	char line[KICMD_CONFIG_LINE_MAX];
	char prefix[KI_UAPI_KFUNC_MAX + 2];

	fp = fopen(KI_USER_CONFIG, "r");
	if (!fp) {
		if (errno == ENOENT) {
			printf("(no persistent configuration)\n");
			return;
		}
		fprintf(stderr, "Error: read %s: %s\n", KI_USER_CONFIG, strerror(errno));
		return;
	}

	if (kfunc && *kfunc)
		snprintf(prefix, sizeof(prefix), "%s.", kfunc);
	else
		prefix[0] = '\0';

	while (fgets(line, sizeof(line), fp)) {
		if (line[0] == '#')
			continue;
		if (prefix[0]) {
			char *eq = strchr(line, '=');
			if (!eq || strncmp(line, prefix, strlen(prefix)))
				continue;
		}
		fputs(line, stdout);
	}
	fclose(fp);
}

int cmd_config(int argc, char **argv)
{
	const char *kfunc;
	const char *key;
	const char *value;
	int ret;

	if (argc < 2 || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
		fputs(kicmd_help_config, stdout);
		return argc < 2 ? 1 : 0;
	}

	if (require_ki_driver() < 0)
		return 1;


	if (!strcmp(argv[1], KICMD_SUB_SET)) {
		if (argc < 5)
			return cli_missing_argument("kicmd config set <kfunc> <key> <value>",
				argc < 3 ? "kfunc" : argc < 4 ? "key" : "value");
		if (argc > 5)
			return cli_unexpected_argument("kicmd config set <kfunc> <key> <value>", argv[5]);
		kfunc = argv[2]; key = argv[3]; value = argv[4];
{
			int fd = open_ki_checked();
			if (fd < 0)
				return 1;
			ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_CONFIG);
			close_ki();
		}
		if (!ret)
			ret = cfg_set(kfunc, key, value);
		if (ret)
			return fprintf(stderr, "Error: save config: %s\n", strerror(-ret)), 1;
		ret = cfg_sync();
		if (ret)
			return ret;
		debug_log("config set %s.%s=%s", kfunc, key, value);
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_UNSET)) {
		if (argc < 4)
			return cli_missing_argument("kicmd config unset <kfunc> <key>",
				argc < 3 ? "kfunc" : "key");
		if (argc > 4)
			return cli_unexpected_argument("kicmd config unset <kfunc> <key>", argv[4]);
		kfunc = argv[2]; key = argv[3];
{
			int fd = open_ki_checked();
			if (fd < 0)
				return 1;
			ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_CONFIG);
			close_ki();
		}
		if (!ret)
			ret = cfg_unset(kfunc, key);
		if (ret)
			return fprintf(stderr, "Error: save config: %s\n", strerror(-ret)), 1;
		ret = cfg_sync();
		if (ret)
			return ret;
		debug_log("config unset %s.%s", kfunc, key);
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_DEL)) {
		if (argc < 3)
			return cli_missing_argument("kicmd config del <kfunc> [key]", "kfunc");
		if (argc > 4)
			return cli_unexpected_argument("kicmd config del <kfunc> [key]", argv[4]);
		if (!argv[2] || !*argv[2] || !valid_token(argv[2]))
			return cli_invalid_argument("kicmd config del <kfunc> [key]", "kfunc");

		kfunc = argv[2];
		key = (argc == 4 && argv[3] && *argv[3]) ? argv[3] : NULL;
		if (key && !valid_token(key))
			return cli_invalid_argument("kicmd config del <kfunc> [key]", "key");
		{
			int fd = open_ki_checked();
			if (fd < 0)
				return 1;
			ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_CONFIG);
			close_ki();
		}
		if (ret)
			return fprintf(stderr, "Error: kfunc '%s' does not support config: %s\n", kfunc, strerror(-ret)), 1;

		if (key && *key)
			ret = cfg_unset(kfunc, key);
		else
			ret = cfg_reset_kfunc(kfunc);
		if (ret)
			return fprintf(stderr, "Error: delete config: %s\n", strerror(-ret)), 1;

		ret = cfg_sync();
		if (ret)
			return ret;
		if (key && *key)
			debug_log("config del %s.%s", kfunc, key);
		else
			debug_log("config del %s", kfunc);
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_RESET)) {
		if (argc > 3)
			return cli_unexpected_argument("kicmd config reset [<kfunc>]", argv[3]);
		if (argc == 3) {
			kfunc = argv[2];
			{
				int fd = open_ki_checked();
				if (fd < 0)
					return 1;
				ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_CONFIG);
				close_ki();
			}
			if (!ret)
				ret = cfg_reset_kfunc(kfunc);
			if (ret)
				return fprintf(stderr, "Error: save config: %s\n", strerror(-ret)), 1;
			ret = cfg_sync();
		if (ret)
			return ret;
		debug_log("config reset %s", kfunc);
			return 0;
		}
		ret = cfg_reset_all();
		if (ret)
			return fprintf(stderr, "Error: save config: %s\n", strerror(-ret)), 1;
		ret = cfg_sync();
		if (ret)
			return ret;
		debug_log("config reset all");
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_ACTIVE) || !strcmp(argv[1], KICMD_SUB_INACTIVE)) {
		bool active = !strcmp(argv[1], KICMD_SUB_ACTIVE);
		if (argc > 2)
			return cli_unexpected_argument(active ? "kicmd config active" : "kicmd config inactive", argv[2]);
		ret = cfg_set_active_and_ioctl(active);
		if (ret)
			return ret;
		debug_log("config %s", argv[1]);
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_LIST)) {
		if (argc > 3)
			return cli_unexpected_argument("kicmd config list [<kfunc>]", argv[3]);
cfg_list(argc == 3 ? argv[2] : NULL);
		return 0;
	}

	{ static const char *const commands[] = { KICMD_SUB_DEL, KICMD_SUB_SET, KICMD_SUB_UNSET, KICMD_SUB_RESET, KICMD_SUB_ACTIVE, KICMD_SUB_INACTIVE, KICMD_SUB_LIST, KICMD_CMD_HELP }; return cli_unknown_command("subcommand", argv[1], "kicmd config <COMMAND>", commands, sizeof(commands) / sizeof(commands[0])); }
}
