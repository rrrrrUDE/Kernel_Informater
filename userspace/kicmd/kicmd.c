#define _GNU_SOURCE 1

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "kicmd_def.h"

static void print_version(void)
{
	printf("Kernel Informater v%s\n", KICMD_VERSION);
}

static void print_help(void)
{
	fputs(kicmd_help, stdout);
}

static int ensure_userd_dir(void)
{
	struct stat st;

	if (!stat(KI_USER_DIR, &st)) {
		if (!S_ISDIR(st.st_mode)) {
			fprintf(stderr, "%s: %s is not a directory\n",
				KICMD_NAME, KI_USER_DIR);
			return -ENOTDIR;
		}
		return 0;
	}

	if (mkdir(KI_USER_DIR, 0700) && errno != EEXIST) {
		fprintf(stderr, "%s: mkdir %s: %s\n",
			KICMD_NAME, KI_USER_DIR, strerror(errno));
		return -errno;
	}

	return 0;
}

static void debug_log(const char *fmt, ...)
{
	FILE *fp;
	va_list ap;
	time_t now;
	struct tm tm;
	char ts[64];

	if (ensure_userd_dir())
		return;

	fp = fopen(KI_USER_DEBUG_LOG, "a");
	if (!fp)
		return;

	now = time(NULL);
	localtime_r(&now, &tm);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
	fprintf(fp, "[%s] ", ts);
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	fputc('\n', fp);
	fclose(fp);
}

static int open_ki(void);
static int ki_ioctl(int fd, unsigned long request, void *arg);

static bool kfunc_exists(const char *kfunc)
{
	static const char *const known_kfuncs[] = { "uname" };
	size_t i;

	if (!kfunc || !*kfunc)
		return false;
	for (i = 0; i < sizeof(known_kfuncs) / sizeof(known_kfuncs[0]); ++i)
		if (!strcmp(kfunc, known_kfuncs[i]))
			return true;
	return false;
}

static int check_kfunc(const char *kfunc)
{
	if (kfunc_exists(kfunc))
		return 0;
	fprintf(stderr, "%s: kfunc '%s' does not exist\n",
		KICMD_NAME, kfunc ? kfunc : "");
	return -ENOENT;
}

static int open_ki(void)
{
	int fd = open(KI_DEVICE_PATH, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		fprintf(stderr, "%s: cannot open %s: %s\n",
			KICMD_NAME, KI_DEVICE_PATH, strerror(errno));
		return -1;
	}

	return fd;
}

static int open_ki_checked(void)
{
	int fd = open_ki();

	if (fd < 0)
		return -1;
	{
		struct ki_ioc_version version;
		memset(&version, 0, sizeof(version));
		if (ki_ioctl(fd, KI_IOC_GET_VERSION, &version) < 0) {
			if (errno == ENOTTY || errno == ENOSYS)
				fprintf(stderr, "%s: Kernel Informater driver is not built in\n", KICMD_NAME);
			else
				fprintf(stderr, "%s: Kernel Informater driver check failed: %s\n",
					KICMD_NAME, strerror(errno));
			close(fd);
			return -1;
		}
	}
	return fd;
}

static int cfg_set_active_and_ioctl(bool active)
{
	int fd;
	int ret;

	fd = open_ki_checked();
	if (fd < 0)
		return 1;

	ret = cfg_set_active(active);
	if (ret) {
		fprintf(stderr, "%s: save config: %s\n",
			KICMD_NAME, strerror(-ret));
		close(fd);
		return 1;
	}

	ret = ki_ioctl(fd, active ? KI_IOC_CONFIG_ON : KI_IOC_CONFIG_OFF, NULL);
	if (ret < 0) {
		fprintf(stderr, "%s: config %s: %s\n",
			KICMD_NAME, active ? "active" : "inactive", strerror(errno));
		close(fd);
		return 1;
	}

	close(fd);
	return 0;
}

static int ki_ioctl(int fd, unsigned long request, void *arg)
{
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

static bool valid_token(const char *s)
{
	const unsigned char *p = (const unsigned char *)s;

	if (!s || !*s)
		return false;
	while (*p) {
		if (*p == '=' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ' ')
			return false;
		p++;
	}
	return true;
}

static bool valid_value(const char *s)
{
	const unsigned char *p = (const unsigned char *)s;

	if (!s || !*s || strlen(s) >= KI_UAPI_VALUE_MAX)
		return false;
	while (*p) {
		if (*p == '\n' || *p == '\r')
			return false;
		p++;
	}
	return true;
}

static bool split_config_line(char *line, char **key, char **value)
{
	char *eq;

	line[strcspn(line, "\r\n")] = '\0';
	if (!*line || line[0] == '#')
		return false;
	eq = strchr(line, '=');
	if (!eq)
		return false;
	*eq = '\0';
	*key = line;
	*value = eq + 1;
	return **key != '\0';
}

static int write_config_with_transform(const char *replace_key,
				       const char *replace_value,
				       const char *remove_prefix,
				       const char *remove_exact,
				       bool reset_all,
				       bool append_new)
{
	FILE *in = NULL;
	FILE *out = NULL;
	int fd = -1;
	char tmp_path[] = KICMD_CONFIG_TMP;
	char line[KICMD_CONFIG_LINE_MAX];
	bool replaced = false;
	int ret;

	ret = ensure_userd_dir();
	if (ret)
		return ret;

	in = fopen(KI_USER_CONFIG, "r");
	if (!in && errno != ENOENT)
		return -errno;

	fd = mkstemp(tmp_path);
	if (fd < 0) {
		ret = -errno;
		if (in)
			fclose(in);
		return ret;
	}

	fchmod(fd, 0600);
	out = fdopen(fd, "w");
	if (!out) {
		ret = -errno;
		close(fd);
		if (in)
			fclose(in);
		unlink(tmp_path);
		return ret;
	}
	fd = -1;

	fprintf(out, "# Kernel Informater persistent configuration\n");

	if (in) {
		while (fgets(line, sizeof(line), in)) {
			char copy[KICMD_CONFIG_LINE_MAX];
			char *key = NULL;
			char *value = NULL;

			strncpy(copy, line, sizeof(copy) - 1);
			copy[sizeof(copy) - 1] = '\0';
			if (!split_config_line(copy, &key, &value)) {
				if (!reset_all && line[0] != '#') {
					fputs(line, out);
				}
				continue;
			}

			if (reset_all)
				continue;
			if (remove_exact && !strcmp(key, remove_exact))
				continue;
			if (remove_prefix && !strncmp(key, remove_prefix, strlen(remove_prefix)))
				continue;
			if (replace_key && !strcmp(key, replace_key)) {
				if (!replaced) {
					fprintf(out, "%s=%s\n", replace_key, replace_value);
					replaced = true;
				}
				continue;
			}
			fprintf(out, "%s=%s\n", key, value);
		}
		fclose(in);
	}

	if (append_new && replace_key && !replaced)
		fprintf(out, "%s=%s\n", replace_key, replace_value);

	if (fflush(out) || fclose(out)) {
		unlink(tmp_path);
		return -EIO;
	}

	if (rename(tmp_path, KI_USER_CONFIG)) {
		ret = -errno;
		unlink(tmp_path);
		return ret;
	}

	chmod(KI_USER_CONFIG, 0600);
	return 0;
}

static int cfg_set(const char *kfunc, const char *key, const char *value)
{
	char compound[KI_UAPI_KFUNC_MAX + KI_UAPI_KEY_MAX + 2];

	if (!valid_token(kfunc) || !valid_token(key) || !valid_value(value))
		return -EINVAL;
	if (snprintf(compound, sizeof(compound), "%s.%s", kfunc, key) >= (int)sizeof(compound))
		return -ENAMETOOLONG;
	return write_config_with_transform(compound, value, NULL, NULL, false, true);
}

static int cfg_unset(const char *kfunc, const char *key)
{
	char compound[KI_UAPI_KFUNC_MAX + KI_UAPI_KEY_MAX + 2];

	if (!valid_token(kfunc) || !valid_token(key))
		return -EINVAL;
	if (snprintf(compound, sizeof(compound), "%s.%s", kfunc, key) >= (int)sizeof(compound))
		return -ENAMETOOLONG;
	return write_config_with_transform(NULL, NULL, NULL, compound, false, false);
}

static int cfg_reset_kfunc(const char *kfunc)
{
	char prefix[KI_UAPI_KFUNC_MAX + 2];

	if (!valid_token(kfunc))
		return -EINVAL;
	snprintf(prefix, sizeof(prefix), "%s.", kfunc);
	return write_config_with_transform(NULL, NULL, prefix, NULL, false, false);
}

static int cfg_reset_all(void)
{
	return write_config_with_transform("active", "0", NULL, NULL, true, true);
}

static int cfg_set_active(bool active)
{
	return write_config_with_transform("active", active ? "1" : "0",
					  NULL, NULL, false, true);
}

static void cfg_list(const char *kfunc)
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
		fprintf(stderr, "%s: read %s: %s\n",
			KICMD_NAME, KI_USER_CONFIG, strerror(errno));
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

static int ioctl_value(unsigned long request,
		       const char *kfunc, const char *key, const char *value)
{
	struct ki_ioc_value v;
	int fd;

	if (check_kfunc(kfunc))
		return 1;
	memset(&v, 0, sizeof(v));
	strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	strncpy(v.key, key, sizeof(v.key) - 1);
	if (value)
		strncpy(v.value, value, sizeof(v.value) - 1);

	fd = open_ki_checked();
	if (fd < 0)
		return 1;
	if (ki_ioctl(fd, request, &v) < 0) {
		fprintf(stderr, "%s: ioctl: %s\n", KICMD_NAME, strerror(errno));
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

	if (check_kfunc(kfunc))
		return 1;
	memset(&v, 0, sizeof(v));
	strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	if (key)
		strncpy(v.key, key, sizeof(v.key) - 1);
	fd = open_ki_checked();
	if (fd < 0)
		return 1;
	if (ki_ioctl(fd, request, &v) < 0) {
		fprintf(stderr, "%s: ioctl: %s\n", KICMD_NAME, strerror(errno));
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}

static int ioctl_kfunc(unsigned long request, const char *kfunc)
{
	struct ki_ioc_kfunc v;
	int fd;

	if (kfunc && *kfunc && check_kfunc(kfunc))
		return 1;
	memset(&v, 0, sizeof(v));
	if (kfunc)
		strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	fd = open_ki_checked();
	if (fd < 0)
		return 1;
	if (ki_ioctl(fd, request, &v) < 0) {
		fprintf(stderr, "%s: ioctl: %s\n", KICMD_NAME, strerror(errno));
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}

static int cmd_safemode(int argc, char **argv)
{
	int ret;

	if (argc < 2 || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
		fputs(kicmd_help_safemode, stdout);
		return argc < 2 ? 1 : 0;
	}

	ret = ensure_userd_dir();
	if (ret)
		return 1;

	if (!strcmp(argv[1], KICMD_SUB_ENABLE)) {
		int fd = open(KI_USER_SAFE_MODE, O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
		if (fd < 0) {
			fprintf(stderr, "%s: create %s: %s\n",
				KICMD_NAME, KI_USER_SAFE_MODE, strerror(errno));
			return 1;
		}
		close(fd);
		debug_log("safemode enable");
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_DISABLE)) {
		if (unlink(KI_USER_SAFE_MODE) && errno != ENOENT) {
			fprintf(stderr, "%s: remove %s: %s\n",
				KICMD_NAME, KI_USER_SAFE_MODE, strerror(errno));
			return 1;
		}
		debug_log("safemode disable");
		return 0;
	}

	return fprintf(stderr, "%s: unknown safemode command: %s\n",
		       KICMD_NAME, argv[1]), 1;
}

static int cmd_config(int argc, char **argv)
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

	if (!strcmp(argv[1], KICMD_SUB_SET)) {
		if (argc != 5)
			return fprintf(stderr, "%s: usage: config set <kfunc> <key> <value>\n", KICMD_NAME), 1;
		kfunc = argv[2]; key = argv[3]; value = argv[4];
		if (check_kfunc(kfunc))
			return 1;
		ret = cfg_set(kfunc, key, value);
		if (ret)
			return fprintf(stderr, "%s: save config: %s\n", KICMD_NAME, strerror(-ret)), 1;
		debug_log("config set %s.%s=%s", kfunc, key, value);
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_UNSET)) {
		if (argc != 4)
			return fprintf(stderr, "%s: usage: config unset <kfunc> <key>\n", KICMD_NAME), 1;
		kfunc = argv[2]; key = argv[3];
		if (check_kfunc(kfunc))
			return 1;
		ret = cfg_unset(kfunc, key);
		if (ret)
			return fprintf(stderr, "%s: save config: %s\n", KICMD_NAME, strerror(-ret)), 1;
		debug_log("config unset %s.%s", kfunc, key);
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_DEL)) {
		if (argc != 3)
			return fprintf(stderr, "%s: usage: config del <kfunc>\n", KICMD_NAME), 1;
		kfunc = argv[2];
		if (check_kfunc(kfunc))
			return 1;
		ret = cfg_reset_kfunc(kfunc);
		if (ret)
			return fprintf(stderr, "%s: save config: %s\n", KICMD_NAME, strerror(-ret)), 1;
		debug_log("config del %s", kfunc);
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_RESET)) {
		if (argc > 3)
			return fprintf(stderr, "%s: usage: config reset [<kfunc>]\n", KICMD_NAME), 1;
		if (argc == 3) {
			kfunc = argv[2];
			if (check_kfunc(kfunc))
				return 1;
			ret = cfg_reset_kfunc(kfunc);
			if (ret)
				return fprintf(stderr, "%s: save config: %s\n", KICMD_NAME, strerror(-ret)), 1;
			debug_log("config reset %s", kfunc);
			return 0;
		}
		ret = cfg_reset_all();
		if (ret)
			return fprintf(stderr, "%s: save config: %s\n", KICMD_NAME, strerror(-ret)), 1;
		debug_log("config reset all");
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_ACTIVE) || !strcmp(argv[1], KICMD_SUB_INACTIVE)) {
		bool active = !strcmp(argv[1], KICMD_SUB_ACTIVE);
		ret = cfg_set_active_and_ioctl(active);
		if (ret)
			return ret;
		debug_log("config %s", argv[1]);
		return 0;
	}

	if (!strcmp(argv[1], KICMD_SUB_LIST)) {
		if (argc > 3)
			return fprintf(stderr, "%s: usage: config list [<kfunc>]\n", KICMD_NAME), 1;
		if (argc == 3 && check_kfunc(argv[2]))
			return 1;
		cfg_list(argc == 3 ? argv[2] : NULL);
		return 0;
	}

	return fprintf(stderr, "%s: unknown config command: %s\n", KICMD_NAME, argv[1]), 1;
}

static int list_real_one(int fd, const char *kfunc, const char *key)
{
	struct ki_ioc_real real;

	memset(&real, 0, sizeof(real));
	strncpy(real.kfunc, kfunc, sizeof(real.kfunc) - 1);
	strncpy(real.key, key, sizeof(real.key) - 1);

	if (ki_ioctl(fd, KI_IOC_GET_REAL_INFO, &real) < 0)
		return -errno;

	printf("%s.%s=%s\n", real.kfunc, real.key, real.value);
	return 0;
}

static int cmd_list(int argc, char **argv)
{
	int fd;
	int i;
	int ret;
	const char *kfunc = NULL;

	if (argc > 2) {
		fputs(kicmd_help_list, stdout);
		return 1;
	}
	if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") ||
			!strcmp(argv[1], "help"))) {
		fputs(kicmd_help_list, stdout);
		return 0;
	}

	if (argc == 2) {
		kfunc = argv[1];
		if (check_kfunc(kfunc))
			return 1;
	}

	fd = open_ki_checked();
	if (fd < 0)
		return 1;

	if (!kfunc || !strcmp(kfunc, "uname")) {
		for (i = 0; i < KICMD_UNAME_KEY_COUNT; ++i) {
			ret = list_real_one(fd, "uname", kicmd_uname_keys[i]);
			if (ret) {
				fprintf(stderr, "%s: list uname.%s: %s\n",
					KICMD_NAME, kicmd_uname_keys[i],
					strerror(-ret));
				close(fd);
				return 1;
			}
		}
	} else {
		ret = list_real_one(fd, kfunc, "release");
		if (ret) {
			fprintf(stderr, "%s: list %s: %s\n",
				KICMD_NAME, kfunc, strerror(-ret));
			close(fd);
			return 1;
		}
	}

	close(fd);
	debug_log("list %s", kfunc ? kfunc : "all");
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

	if (!strcmp(argv[1], KICMD_SUB_SET)) {
		if (argc != 5)
			return fprintf(stderr, "%s: usage: func set <kfunc> <key> <value>\n", KICMD_NAME), 1;
		ret = ioctl_value(KI_IOC_FUNC_VALUE_SET, argv[2], argv[3], argv[4]);
		debug_log("func set %s.%s=%s", argv[2], argv[3], argv[4]);
		return ret;
	}

	if (!strcmp(argv[1], KICMD_SUB_UNSET)) {
		if (argc != 4)
			return fprintf(stderr, "%s: usage: func unset <kfunc> <key>\n", KICMD_NAME), 1;
		ret = ioctl_key(KI_IOC_FUNC_VALUE_UNSET, argv[2], argv[3]);
		debug_log("func unset %s.%s", argv[2], argv[3]);
		return ret;
	}

	if (!strcmp(argv[1], KICMD_SUB_RESET)) {
		if (argc > 3)
			return fprintf(stderr, "%s: usage: func reset [<kfunc>]\n", KICMD_NAME), 1;
		ret = ioctl_kfunc(KI_IOC_FUNC_KFUNC_RESET, argc == 3 ? argv[2] : "");
		debug_log("func reset %s", argc == 3 ? argv[2] : "all");
		return ret;
	}

	return fprintf(stderr, "%s: unknown func command: %s\n", KICMD_NAME, argv[1]), 1;
}

static int cmd_help(int argc, char **argv)
{
	if (argc < 2) {
		print_help();
		return 0;
	}
	if (!strcmp(argv[1], KICMD_CMD_SAFEMODE)) fputs(kicmd_help_safemode, stdout);
	else if (!strcmp(argv[1], KICMD_CMD_CONFIG)) fputs(kicmd_help_config, stdout);
	else if (!strcmp(argv[1], KICMD_CMD_LIST)) fputs(kicmd_help_list, stdout);
	else if (!strcmp(argv[1], KICMD_CMD_FUNC)) fputs(kicmd_help_func, stdout);
	else return fprintf(stderr, "%s: unknown command: %s\n", KICMD_NAME, argv[1]), 1;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		print_help();
		return 0;
	}
	if (!strcmp(argv[1], KICMD_CMD_HELP)) return cmd_help(argc - 1, argv + 1);
	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) { print_help(); return 0; }
	if (!strcmp(argv[1], KICMD_CMD_VERSION) || !strcmp(argv[1], "-V") || !strcmp(argv[1], "--version")) { print_version(); return 0; }
	if (!strcmp(argv[1], KICMD_CMD_SAFEMODE)) return cmd_safemode(argc - 1, argv + 1);
	if (!strcmp(argv[1], KICMD_CMD_CONFIG)) return cmd_config(argc - 1, argv + 1);
	if (!strcmp(argv[1], KICMD_CMD_LIST)) return cmd_list(argc - 1, argv + 1);
	if (!strcmp(argv[1], KICMD_CMD_FUNC)) return cmd_func(argc - 1, argv + 1);
	fprintf(stderr, "%s: unknown command: %s\n", KICMD_NAME, argv[1]);
	fprintf(stderr, "%s: try '%s help'\n", KICMD_NAME, KICMD_NAME);
	return 1;
}
