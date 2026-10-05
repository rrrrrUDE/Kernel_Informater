#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <unistd.h>

int filesystem_mount(int argc, char **argv)
{
	struct ki_ioc_filesystem_mount request;
	int fd;
	int ret;

	if (argc < 2)
		return cli_missing_argument("kicmd func filesystem mount <COMMAND>", "COMMAND");

	if (!strcmp(argv[1], KICMD_CMD_HELP) || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help")) {
		fputs("Usage: kicmd func filesystem mount <COMMAND>\\n\\n"
		      "Commands:\\n"
		      "  add <source> <target>       Add a bind mount\\n"
		      "  umount [--lazy|-l] <target> Unmount a mount point\\n"
		      "  help                        Print help\\n\\n"
		      "Options:\\n"
		      "  -h, --help                 Print help\\n"
		      "  -l, --lazy                Lazy/thermal unmount\\n", stdout);
		return 0;
	}

	memset(&request, 0, sizeof(request));

	if (!strcmp(argv[1], "add")) {
		if (argc < 4)
			return cli_missing_argument("kicmd func filesystem mount add <source> <target>",
				argc < 3 ? "source" : "target");
		if (argc > 4)
			return cli_unexpected_argument(
				"kicmd func filesystem mount add <source> <target>", argv[4]);

		request.operation = KI_FILESYSTEM_MOUNT_ADD;
		request.flags = MS_BIND;
		request.source = (__u64)(unsigned long)argv[2];
		request.target = (__u64)(unsigned long)argv[3];
	} else if (!strcmp(argv[1], "umount")) {
		bool lazy = false;
		const char *target;

		if (argc < 3)
			return cli_missing_argument(
				"kicmd func filesystem mount umount [--lazy|-l] <target>",
				"target");
		if (argc > 4)
			return cli_unexpected_argument(
				"kicmd func filesystem mount umount [--lazy|-l] <target>",
				argv[4]);

		if (!strcmp(argv[2], "-l") || !strcmp(argv[2], "--lazy")) {
			lazy = true;
			if (argc < 4)
				return cli_missing_argument(
					"kicmd func filesystem mount umount [--lazy|-l] <target>",
					"target");
			target = argv[3];
		} else {
			if (argc != 3)
				return cli_unexpected_argument(
					"kicmd func filesystem mount umount [--lazy|-l] <target>",
					argv[3]);
			target = argv[2];
		}

		request.operation = KI_FILESYSTEM_MOUNT_UMOUNT;
		request.flags = lazy ? MNT_DETACH : 0;
		request.target = (__u64)(unsigned long)target;
	} else {
		static const char *const commands[] = {
			"add", "umount", KICMD_CMD_HELP
		};
		return cli_unknown_command("subcommand", argv[1], 
			"kicmd func filesystem mount <COMMAND>", commands,
			sizeof(commands) / sizeof(commands[0]));
	}

	fd = open_ki_checked();
	if (fd < 0)
		return 1;
	ret = ki_ioctl(KI_IOC_FILESYSTEM_MOUNT, &request);
	close_ki();
	if (ret < 0)
		return -errno;

	if (request.operation == KI_FILESYSTEM_MOUNT_ADD)
		printf("- Mounted %s -> %s\\n", argv[2], argv[3]);
	else {
		const char *target = request.flags & MNT_DETACH ?
			((argc >= 4 && (!strcmp(argv[2], "-l") || !strcmp(argv[2], "--lazy"))) ?
				argv[3] : argv[2]) : argv[2];
		printf("- Unmounted %s%s\\n", target,
		       request.flags & MNT_DETACH ? " (lazy)" : "");
	}

	return 0;
}

int filesystem_func(int argc, char **argv)
{
	int fd;
	struct ki_ioc_real real;

	if (argc < 2)
		return cli_missing_argument("kicmd func filesystem <COMMAND>", "COMMAND");

	if (!strcmp(argv[1], KICMD_CMD_HELP) || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help")) {
		fputs("Usage: kicmd func filesystem <COMMAND>\n\n"
		      "Commands:\n"
		      "  stat <path>              Show filesystem information\n"
		      "  mount <COMMAND>          Manage mount operations\n"
		      "  help                     Print help\n\n"
		      "Options:\n"
		      "  -h, --help   Print help\n", stdout);
		return 0;
	}
	if (!strcmp(argv[1], "mount"))
		return filesystem_mount(argc - 1, argv + 1);

	if (strcmp(argv[1], "stat")) {
		static const char *const commands[] = { "stat", KICMD_CMD_HELP };
		return cli_unknown_command("subcommand", argv[1],
			"kicmd func filesystem <COMMAND>", commands,
			sizeof(commands) / sizeof(commands[0]));
	}
	if (argc < 3)
		return cli_missing_argument("kicmd func filesystem stat <path>", "path");
	if (argc > 3)
		return cli_unexpected_argument("kicmd func filesystem stat <path>", argv[3]);

	if (strlen(argv[2]) + 5 >= KI_UAPI_KEY_MAX)
		return cli_invalid_argument("kicmd func filesystem stat <path>", "path");

	fd = open_ki_checked();
	if (fd < 0)
		return 1;

	memset(&real, 0, sizeof(real));
	strncpy(real.kfunc, "filesystem", sizeof(real.kfunc) - 1);
	snprintf(real.key, sizeof(real.key), "stat:%s", argv[2]);

	if (ki_ioctl( KI_IOC_GET_REAL_INFO, &real) < 0) {
		int saved_errno = errno;
		close_ki();
		return -saved_errno;
	}

	close_ki();
	printf("%s.%s=%s\n", real.kfunc, real.key, real.value);
	return 0;
}#include "../../include/kicmd_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <unistd.h>

int filesystem_mount(int argc, char **argv)
{
	struct ki_ioc_filesystem_mount request;
	int fd;
	int ret;

	if (argc < 2)
		return cli_missing_argument("kicmd func filesystem mount <COMMAND>", "COMMAND");

	if (!strcmp(argv[1], KICMD_CMD_HELP) || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help")) {
		fputs("Usage: kicmd func filesystem mount <COMMAND>\\n\\n"
		      "Commands:\\n"
		      "  add <source> <target>       Add a bind mount\\n"
		      "  umount [--lazy|-l] <target> Unmount a mount point\\n"
		      "  help                        Print help\\n\\n"
		      "Options:\\n"
		      "  -h, --help                 Print help\\n"
		      "  -l, --lazy                Lazy/thermal unmount\\n", stdout);
		return 0;
	}

	memset(&request, 0, sizeof(request));

	if (!strcmp(argv[1], "add")) {
		if (argc < 4)
			return cli_missing_argument("kicmd func filesystem mount add <source> <target>",
				argc < 3 ? "source" : "target");
		if (argc > 4)
			return cli_unexpected_argument(
				"kicmd func filesystem mount add <source> <target>", argv[4]);

		request.operation = KI_FILESYSTEM_MOUNT_ADD;
		request.flags = MS_BIND;
		request.source = (__u64)(unsigned long)argv[2];
		request.target = (__u64)(unsigned long)argv[3];
	} else if (!strcmp(argv[1], "umount")) {
		bool lazy = false;
		const char *target;

		if (argc < 3)
			return cli_missing_argument(
				"kicmd func filesystem mount umount [--lazy|-l] <target>",
				"target");
		if (argc > 4)
			return cli_unexpected_argument(
				"kicmd func filesystem mount umount [--lazy|-l] <target>",
				argv[4]);

		if (!strcmp(argv[2], "-l") || !strcmp(argv[2], "--lazy")) {
			lazy = true;
			if (argc < 4)
				return cli_missing_argument(
					"kicmd func filesystem mount umount [--lazy|-l] <target>",
					"target");
			target = argv[3];
		} else {
			if (argc != 3)
				return cli_unexpected_argument(
					"kicmd func filesystem mount umount [--lazy|-l] <target>",
					argv[3]);
			target = argv[2];
		}

		request.operation = KI_FILESYSTEM_MOUNT_UMOUNT;
		request.flags = lazy ? MNT_DETACH : 0;
		request.target = (__u64)(unsigned long)target;
	} else {
		static const char *const commands[] = {
			"add", "umount", KICMD_CMD_HELP
		};
		return cli_unknown_command("subcommand", argv[1], 
			"kicmd func filesystem mount <COMMAND>", commands,
			sizeof(commands) / sizeof(commands[0]));
	}

	fd = open_ki_checked();
	if (fd < 0)
		return 1;
	ret = ki_ioctl(KI_IOC_FILESYSTEM_MOUNT, &request);
	close_ki();
	if (ret < 0)
		return -errno;

	if (request.operation == KI_FILESYSTEM_MOUNT_ADD)
		printf("- Mounted %s -> %s\\n", argv[2], argv[3]);
	else {
		const char *target = request.flags & MNT_DETACH ?
			((argc >= 4 && (!strcmp(argv[2], "-l") || !strcmp(argv[2], "--lazy"))) ?
				argv[3] : argv[2]) : argv[2];
		printf("- Unmounted %s%s\\n", target,
		       request.flags & MNT_DETACH ? " (lazy)" : "");
	}

	return 0;
}

int filesystem_func(int argc, char **argv)
{
	int fd;
	struct ki_ioc_real real;

	if (argc < 2)
		return cli_missing_argument("kicmd func filesystem <COMMAND>", "COMMAND");

	if (!strcmp(argv[1], KICMD_CMD_HELP) || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help")) {
		fputs("Usage: kicmd func filesystem <COMMAND>\n\n"
		      "Commands:\n"
		      "  stat <path>              Show filesystem information\n"
		      "  mount <COMMAND>          Manage mount operations\n"
		      "  help                     Print help\n\n"
		      "Options:\n"
		      "  -h, --help   Print help\n", stdout);
		return 0;
	}
	if (!strcmp(argv[1], "mount"))
		return filesystem_mount(argc - 1, argv + 1);

	if (strcmp(argv[1], "stat")) {
		static const char *const commands[] = { "stat", KICMD_CMD_HELP };
		return cli_unknown_command("subcommand", argv[1],
			"kicmd func filesystem <COMMAND>", commands,
			sizeof(commands) / sizeof(commands[0]));
	}
	if (argc < 3)
		return cli_missing_argument("kicmd func filesystem stat <path>", "path");
	if (argc > 3)
		return cli_unexpected_argument("kicmd func filesystem stat <path>", argv[3]);

	if (strlen(argv[2]) + 5 >= KI_UAPI_KEY_MAX)
		return cli_invalid_argument("kicmd func filesystem stat <path>", "path");

	fd = open_ki_checked();
	if (fd < 0)
		return 1;

	memset(&real, 0, sizeof(real));
	strncpy(real.kfunc, "filesystem", sizeof(real.kfunc) - 1);
	snprintf(real.key, sizeof(real.key), "stat:%s", argv[2]);

	if (ki_ioctl( KI_IOC_GET_REAL_INFO, &real) < 0) {
		int saved_errno = errno;
		close_ki();
		return -saved_errno;
	}

	close_ki();
	printf("%s.%s=%s\n", real.kfunc, real.key, real.value);
	return 0;
}