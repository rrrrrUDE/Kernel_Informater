#define _GNU_SOURCE 1

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../kicmd_internal.h"

 size_t cli_edit_distance(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b), i, j;
    size_t prev[65], cur[65];
    if (la > 64) la = 64;
    if (lb > 64) lb = 64;
    for (j = 0; j <= lb; j++) prev[j] = j;
    for (i = 1; i <= la; i++) {
        cur[0] = i;
        for (j = 1; j <= lb; j++) {
            size_t v = prev[j] + 1;
            size_t x = cur[j - 1] + 1;
            size_t y = prev[j - 1] + (a[i - 1] != b[j - 1]);
            if (x < v) v = x;
            if (y < v) v = y;
            cur[j] = v;
        }
        memcpy(prev, cur, (lb + 1) * sizeof(prev[0]));
    }
    return prev[lb];
}

 bool cli_command_matches(const char *input, const char *candidate)
{
    size_t len;
    if (!input || !candidate || !*input || !*candidate) return false;
    len = strlen(input);
    return !strncmp(candidate, input, len) ||
           cli_edit_distance(input, candidate) <= (len <= 3 ? 1 : 2);
}

 void cli_print_suggestions(const char *input, const char *const *commands, size_t count)
{
	size_t i;
	size_t matches = 0;
	const char *last = NULL;

	for (i = 0; i < count; i++) {
		if (cli_command_matches(input, commands[i])) {
			matches++;
			last = commands[i];
		}
	}

	if (!matches)
		return;

	if (matches == 1)
		fprintf(stderr, "\n  tip: a similar command exists: '%s'\n", last);
	else {
		fputs("\n  tip: some similar commands exist: ", stderr);
		matches = 0;
		for (i = 0; i < count; i++) {
			if (!cli_command_matches(input, commands[i]))
				continue;
			if (matches++)
				fputs(", ", stderr);
			fprintf(stderr, "'%s'", commands[i]);
		}
		fputc('\n', stderr);
	}
}

 int cli_unknown_command(const char *scope, const char *command, const char *usage, const char *const *commands, size_t count)
{
	fprintf(stderr, "error: unrecognized %s '%s'\n", scope, command ? command : "");
	cli_print_suggestions(command, commands, count);
	fprintf(stderr, "\nUsage: %s\n\n", usage);
	fprintf(stderr, "For more information, try '--help'.\n");
	return 1;
}

 int cli_missing_argument(const char *usage, const char *argument)
{
	fprintf(stderr,
		"error: the following required arguments were not provided:\n"
		"  <%s>\n\n"
		"Usage: %s\n\n"
		"For more information, try '--help'.\n",
		argument, usage);
	return 1;
}

 int cli_unexpected_argument(const char *usage, const char *argument)
{
	fprintf(stderr,
		"error: unexpected argument '%s'\n\n"
		"Usage: %s\n\n"
		"For more information, try '--help'.\n",
		argument ? argument : "", usage);
	return 1;
}

 int cli_invalid_argument(const char *usage, const char *argument)
{
	fprintf(stderr,
		"error: invalid value for <%s>\n\n"
		"Usage: %s\n\n"
		"For more information, try '--help'.\n",
		argument ? argument : "argument", usage);
	return 1;
}

 int cli_result(int ret)
{
	if (ret < 0) {
		fprintf(stderr, "Error: %s\n", strerror(-ret));
		return 1;
	}
	return ret;
}

 int open_ki_checked(void);

 int require_ki_driver(void)
{
	if (open_ki_checked() < 0)
		return -ENODEV;
	return 0;
}

 int ki_ioctl(unsigned long request, void *arg);
 void close_ki(void);
static int ki_driver_fd = -1;
static bool ki_driver_checked;
 int ensure_userd_dir(void);
 bool ki_debug_enabled(void);
 int ioctl_value(unsigned long request,
			       const char *kfunc, const char *key, const char *value);
 int open_ki_checked(void);


 void module_report_error(const char *operation, const char *name, int error)
{
	const char *reason = strerror(error);
	int kmsg = -1;
	char buf[4096];
	ssize_t n;

	fprintf(stderr, "Error: %s %s: %s\n",
		operation, name ? name : "module", reason);

	/*
	 * Like ksud, inspect the kernel log after a module-load failure so
	 * users get the actual kernel-side reason (vermagic, unknown symbol,
	 * invalid format, etc.) instead of only errno.
	 */
	if (strcmp(operation, "insmod"))
		return;

	kmsg = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (kmsg < 0)
		return;

	while ((n = read(kmsg, buf, sizeof(buf) - 1)) > 0) {
		buf[n] = '\0';
		for (char *line = buf; line;) {
			char *next = strchr(line, '\n');
			if (next)
				*next++ = '\0';
			if (strstr(line, "Unknown symbol") ||
			    strstr(line, "version magic") ||
			    strstr(line, "invalid module") ||
			    strstr(line, "module verification failed")) {
				fprintf(stderr, "Error: kernel: %s\n", line);
			}
			line = next;
			if (!next)
				break;
		}
	}

	close(kmsg);
}

 void debug_log(const char *fmt, ...)
{
	FILE *fp;
	va_list ap;
	time_t now;
	struct tm tm;
	char ts[64];

	if (!ki_debug_enabled())
		return;

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







 int cmd_list_process(int argc, char **argv);
 int cmd_func_process(int argc, char **argv);
 int check_kfunc_feature( const char *kfunc, unsigned int feature)
{
	struct ki_ioc_kfunc_features info;

	if (!kfunc || !*kfunc)
		return -EINVAL;

	memset(&info, 0, sizeof(info));
	strncpy(info.kfunc, kfunc, sizeof(info.kfunc) - 1);

	if (ki_ioctl( KI_IOC_GET_KFUNC_FEATURES, &info) < 0)
		return -errno;
	if (!(info.features & feature))
		return -EOPNOTSUPP;
	return 0;
}


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

 int ensure_userd_dir(void)
{
	struct stat st;

	if (!stat(KI_USER_DIR, &st)) {
		if (!S_ISDIR(st.st_mode)) {
			fprintf(stderr, "Error: %s is not a directory\n", KI_USER_DIR);
			return -ENOTDIR;
		}
		return 0;
	}

	if (mkdir(KI_USER_DIR, 0700) && errno != EEXIST) {
		fprintf(stderr, "Error: mkdir %s: %s\n", KI_USER_DIR, strerror(errno));
		return -errno;
	}

	return 0;
}

 bool ki_debug_enabled(void)
{
	static int cached = -1;
	struct ki_ioc_debug debug;

	if (cached >= 0)
		return cached != 0;

	if (open_ki_checked() < 0) {
		cached = 0;
		return false;
	}

	memset(&debug, 0, sizeof(debug));
	if (ki_ioctl(KI_IOC_GET_DEBUG, &debug) < 0) {
		cached = 0;
		return false;
	}

	cached = debug.enabled ? 1 : 0;
	return cached != 0;
}

 int open_ki(void)
{
	if (ki_driver_fd >= 0)
		return ki_driver_fd;

	ki_driver_fd = open(KI_DEVICE_PATH, O_RDWR | O_CLOEXEC);
	if (ki_driver_fd < 0) {
		fprintf(stderr, "Error: Kernel Informater driver is not built in\n");
		return -1;
	}

	return ki_driver_fd;
}

 int open_ki_checked(void)
{
	struct ki_ioc_version version;

	if (ki_driver_checked && ki_driver_fd >= 0)
		return ki_driver_fd;
	if (open_ki() < 0)
		return -1;

	memset(&version, 0, sizeof(version));
	if (ki_ioctl(KI_IOC_GET_VERSION, &version) < 0) {
		fprintf(stderr, "Error: Kernel Informater driver check failed: %s\n", strerror(errno));
		close(ki_driver_fd);
		ki_driver_fd = -1;
		return -1;
	}
	ki_driver_checked = true;
	return ki_driver_fd;
}

 void close_ki(void)
{
	if (ki_driver_fd >= 0)
		close(ki_driver_fd);
	ki_driver_fd = -1;
	ki_driver_checked = false;
}

 int cfg_set_active(bool active);
 int cfg_sync(void);
 int cfg_set_active_and_ioctl(bool active);





 int ki_ioctl(unsigned long request, void *arg)
{
	int ret;

	if (ki_driver_fd < 0) {
		errno = ENODEV;
		return -1;
	}

	do {
		ret = ioctl(ki_driver_fd, request, arg);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

 bool valid_token(const char *s)
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

 bool valid_value(const char *s)
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

 bool split_config_line(char *line, char **key, char **value)
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

 int write_config_with_transform(const char *replace_key,
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
				continue;			}

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





























 int parse_pid(const char *s, pid_t *pid)
{
	char *endp;
	long value;

	if (!s || !*s || !pid)
		return -EINVAL;
	errno = 0;
	value = strtol(s, &endp, 10);
	if (errno || *endp || value <= 0 || value > INT_MAX)
		return -EINVAL;
	*pid = (pid_t)value;
	return 0;
}

 int parse_u64(const char *s, unsigned long long *value)
{
	char *endp;

	if (!s || !*s || !value)
		return -EINVAL;
	errno = 0;
	*value = strtoull(s, &endp, 0);
	if (errno || *endp)
		return -EINVAL;
	return 0;
}

 int cli_parse_pid(const char *usage, const char *argument,
			 const char *value, pid_t *pid)
{
	int ret = parse_pid(value, pid);
	if (ret)
		return cli_invalid_argument(usage, argument);
	return 0;
}

 int cli_parse_u64(const char *usage, const char *argument,
			  const char *value, unsigned long long *number)
{
	int ret = parse_u64(value, number);
	if (ret)
		return cli_invalid_argument(usage, argument);
	return 0;
}



















