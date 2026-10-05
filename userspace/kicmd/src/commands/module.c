#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <unistd.h>

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

int module_func(int argc, char **argv)
{
	int fd;
	int ret;

	if (argc < 2)
		return cli_missing_argument("kicmd func module <COMMAND>", "COMMAND");

	if (!strcmp(argv[1], KICMD_CMD_HELP) || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help")) {
		fputs("Usage: kicmd func module <COMMAND>\n\n"
		      "Commands:\n"
		      "  insmod <path> [args...]  Load a kernel module\n"
		      "  rmmod <name>             Remove a kernel module\n"
		      "  help                     Print help\n\n"
		      "Options:\n"
		      "  -h, --help               Print help\n", stdout);
		return 0;
	}

	if (!strcmp(argv[1], "rmmod")) {
		if (argc < 3)
			return cli_missing_argument("kicmd func module rmmod <name>", "name");
		if (argc > 3)
			return cli_unexpected_argument("kicmd func module rmmod <name>", argv[3]);
		if (!argv[2][0])
			return cli_missing_argument("kicmd func module rmmod <name>", "name");
#ifdef SYS_delete_module
		ret = (int)syscall(SYS_delete_module, argv[2], 0);
		if (ret < 0) {
			int error = errno;
			module_report_error("rmmod", argv[2], error);
			return -error;
		}
		debug_log("module rmmod %s", argv[2]);
		return 0;
#else
		return -ENOSYS;
#endif
	}

	if (strcmp(argv[1], "insmod")) {
		static const char *const commands[] = { "insmod", "rmmod", KICMD_CMD_HELP };
		return cli_unknown_command("subcommand", argv[1],
			"kicmd func module <COMMAND>", commands,
			sizeof(commands) / sizeof(commands[0]));
	}
	if (argc < 3)
		return cli_missing_argument("kicmd func module insmod <path> [args...]", "path");

	fd = open(argv[2], O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

#ifdef SYS_finit_module
	{
		char value[KI_UAPI_VALUE_MAX];
		size_t used = 0;
		int i;

		memset(value, 0, sizeof(value));
		for (i = 3; i < argc; i++) {
			size_t n = strlen(argv[i]);
			if (n + used + (used ? 1 : 0) >= sizeof(value)) {
				close(fd);
				return -E2BIG;
			}
			if (used)
				value[used++] = ' ';
			memcpy(value + used, argv[i], n);
			used += n;
		}
		ret = (int)syscall(SYS_finit_module, fd, value, 0);
	}
#else
	ret = -1;
	errno = ENOSYS;
#endif
	close(fd);
	if (ret < 0) {
		int error = errno;
		module_report_error("insmod", argv[2], error);
		return 1;
	}
	debug_log("module insmod %s", argv[2]);
	return 0;
}#include "../../include/kicmd_internal.h"
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

int module_func(int argc, char **argv)
{
	int fd;
	int ret;

	if (argc < 2)
		return cli_missing_argument("kicmd func module <COMMAND>", "COMMAND");

	if (!strcmp(argv[1], KICMD_CMD_HELP) || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help")) {
		fputs("Usage: kicmd func module <COMMAND>\n\n"
		      "Commands:\n"
		      "  insmod <path> [args...]  Load a kernel module\n"
		      "  rmmod <name>             Remove a kernel module\n"
		      "  help                     Print help\n\n"
		      "Options:\n"
		      "  -h, --help               Print help\n", stdout);
		return 0;
	}

	if (!strcmp(argv[1], "rmmod")) {
		if (argc < 3)
			return cli_missing_argument("kicmd func module rmmod <name>", "name");
		if (argc > 3)
			return cli_unexpected_argument("kicmd func module rmmod <name>", argv[3]);
		if (!argv[2][0])
			return cli_missing_argument("kicmd func module rmmod <name>", "name");
#ifdef SYS_delete_module
		ret = (int)syscall(SYS_delete_module, argv[2], 0);
		if (ret < 0) {
			int error = errno;
			module_report_error("rmmod", argv[2], error);
			return -error;
		}
		debug_log("module rmmod %s", argv[2]);
		return 0;
#else
		return -ENOSYS;
#endif
	}

	if (strcmp(argv[1], "insmod")) {
		static const char *const commands[] = { "insmod", "rmmod", KICMD_CMD_HELP };
		return cli_unknown_command("subcommand", argv[1],
			"kicmd func module <COMMAND>", commands,
			sizeof(commands) / sizeof(commands[0]));
	}
	if (argc < 3)
		return cli_missing_argument("kicmd func module insmod <path> [args...]", "path");

	fd = open(argv[2], O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

#ifdef SYS_finit_module
	{
		char value[KI_UAPI_VALUE_MAX];
		size_t used = 0;
		int i;

		memset(value, 0, sizeof(value));
		for (i = 3; i < argc; i++) {
			size_t n = strlen(argv[i]);
			if (n + used + (used ? 1 : 0) >= sizeof(value)) {
				close(fd);
				return -E2BIG;
			}
			if (used)
				value[used++] = ' ';
			memcpy(value + used, argv[i], n);
			used += n;
		}
		ret = (int)syscall(SYS_finit_module, fd, value, 0);
	}
#else
	ret = -1;
	errno = ENOSYS;
#endif
	close(fd);
	if (ret < 0) {
		int error = errno;
		module_report_error("insmod", argv[2], error);
		return 1;
	}
	debug_log("module insmod %s", argv[2]);
	return 0;
}