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

int process_check_func(void)
{
	return check_kfunc_feature("process", KI_KFUNC_FEATURE_FUNC);
}

int list_process(void)
{
	struct ki_ioc_process_entry entry;
	unsigned int index = 0;

	printf("PID\tPPID\tUID\tSTATE\tNAME\n");
	for (;;) {
		memset(&entry, 0, sizeof(entry));
		entry.index = index++;
		if (ki_ioctl( KI_IOC_PROCESS_LIST, &entry) < 0) {
			if (errno == ENOENT && index > 0)
				return 0;
			return -errno;
		}
		printf("%d\t%d\t%u\t%c\t%s\n",
			entry.pid, entry.ppid, entry.uid,
			(char)entry.state, entry.comm);
	}
}

int process_info( pid_t pid)
{
	struct ki_ioc_process_info info;

	memset(&info, 0, sizeof(info));
	info.pid = pid;
	if (ki_ioctl( KI_IOC_PROCESS_INFO, &info) < 0)
		return -errno;

	printf("pid:%d\n", info.pid);
	printf("tgid:%d\n", info.tgid);
	printf("ppid:%d\n", info.ppid);
	printf("uid:%u\n", info.uid);
	printf("gid:%u\n", info.gid);
	printf("state:%c\n", (char)info.state);
	printf("flags:0x%x\n", info.flags);
	printf("start_time:%llu\n", (unsigned long long)info.start_time);
	printf("virtual_size:%llu\n", (unsigned long long)info.virtual_size);
	printf("resident_pages:%llu\n", (unsigned long long)info.resident_pages);
	printf("comm:%s\n", info.comm);
	return 0;
}

int process_read_memory( pid_t pid,
			       unsigned long long address, unsigned int size)
{
	struct ki_ioc_process_read read;
	unsigned int i;

	if (!size || size > KI_UAPI_PROCESS_READ_MAX)
		return -EINVAL;

	memset(&read, 0, sizeof(read));
	read.pid = pid;
	read.address = address;
	read.size = size;

	if (ki_ioctl( KI_IOC_PROCESS_READ_MEMORY, &read) < 0)
		return -errno;

	for (i = 0; i < read.size; i++) {
		if (i && !(i % 16))
			putchar('\n');
		printf("%02x", read.data[i]);
	}
	putchar('\n');
	return 0;
}

int process_signal( pid_t pid, bool tree)
{
	struct ki_ioc_process_pid request;

	memset(&request, 0, sizeof(request));
	request.pid = pid;
	if (ki_ioctl( tree ? KI_IOC_PROCESS_KILL_TREE : KI_IOC_PROCESS_KILL,
		     &request) < 0)
		return -errno;
	return 0;
}

int cmd_list_process(int argc, char **argv)
{
	pid_t pid;
	int ret;

	if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") ||
			!strcmp(argv[1], "help"))) {
		fputs("Usage: kicmd list process [<pid>]\n\n"
		      "Show all visible processes or information for one PID.\n\n"
		      "Options:\n"
		      "  -h, --help   Print help\n", stdout);
		return 0;
	}

	if (argc > 2)
		return cli_unexpected_argument("kicmd list process [<pid>]", argv[2]);

	if (argc == 2) {
		ret = cli_parse_pid("kicmd list process [<pid>]", "pid", argv[1], &pid);
		if (ret)
			return ret;
		ret = process_info(pid);
	} else if (argc == 1) {
		ret = list_process();
	} else {
		return cli_unexpected_argument("kicmd list process [<pid>]", argv[2]);
	}

	return ret;
}

int cmd_func_process(int argc, char **argv)
{
	pid_t pid;
	unsigned long long address;
	char *endp;
	unsigned long size;
	int fd;
	int ret;

	if (argc < 2)
		return cli_missing_argument("kicmd func process <COMMAND>", "COMMAND");

	if (!strcmp(argv[1], KICMD_CMD_HELP) || !strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help")) {
		fputs("Usage: kicmd func process <COMMAND>\n\n"
		      "Commands:\n"
		      "  info <pid>                         Show process information\n"
		      "  read_memory <pid> <address> <size> Read process memory\n"
		      "  kill <pid>                         Kill one process\n"
		      "  kill_tree <pid>                    Kill a process and its descendants\n"
		      "  help                               Print help\n\n"
		      "Options:\n"
		      "  -h, --help                         Print help\n", stdout);
		return 0;
	}

	fd = open_ki_checked();
	if (fd < 0)
		return -ENODEV;

	ret = process_check_func();
	if (ret) {
		close_ki();
		return ret;
	}

	if (!strcmp(argv[1], "info")) {
		if (argc < 3)
			return cli_missing_argument("kicmd func process info <pid>", "pid");
		if (argc > 3)
			return cli_unexpected_argument("kicmd func process info <pid>", argv[3]);
		ret = cli_parse_pid("kicmd func process info <pid>", "pid", argv[2], &pid);
		if (!ret)
			ret = process_info(pid);
	} else if (!strcmp(argv[1], "read_memory")) {
		if (argc < 5)
			return cli_missing_argument("kicmd func process read_memory <pid> <address> <size>",
				argc < 3 ? "pid" : argc < 4 ? "address" : "size");
		if (argc > 5)
			return cli_unexpected_argument("kicmd func process read_memory <pid> <address> <size>", argv[5]);
		ret = cli_parse_pid("kicmd func process read_memory <pid> <address> <size>",
			"pid", argv[2], &pid);
		if (!ret)
			ret = cli_parse_u64("kicmd func process read_memory <pid> <address> <size>",
				"address", argv[3], &address);
		if (!ret) {
			errno = 0;
			size = strtoul(argv[4], &endp, 0);
			if (errno || *endp || !size || size > KI_UAPI_PROCESS_READ_MAX)
				return cli_invalid_argument(
					"kicmd func process read_memory <pid> <address> <size>", "size");
			ret = process_read_memory(pid, address, (unsigned int)size);
		}
	} else if (!strcmp(argv[1], "kill") || !strcmp(argv[1], "kill_tree")) {
		const char *usage = !strcmp(argv[1], "kill") ?
			"kicmd func process kill <pid>" :
			"kicmd func process kill_tree <pid>";
		if (argc < 3)
			return cli_missing_argument(usage, "pid");
		if (argc > 3)
			return cli_unexpected_argument(usage, argv[3]);
		ret = cli_parse_pid(usage, "pid", argv[2], &pid);
		if (!ret)
			ret = process_signal(pid, !strcmp(argv[1], "kill_tree"));
	} else {
		static const char *const commands[] = {
			"info", "read_memory", "kill", "kill_tree", KICMD_CMD_HELP
		};
		close_ki();
		return cli_unknown_command("subcommand", argv[1],
			"kicmd func process <COMMAND>", commands,
			sizeof(commands) / sizeof(commands[0]));
	}

	close_ki();
	return ret;
}
