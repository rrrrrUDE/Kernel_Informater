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

static int list_kernel_lines( unsigned int type)
{
	struct ki_ioc_list_line line;
	unsigned int index = 0;

	for (;;) {
		memset(&line, 0, sizeof(line));
		line.type = type;
		line.index = index;
		if (ki_ioctl( KI_IOC_LIST_LINE, &line) < 0) {
			if (errno == ENOENT)
				return 0;
			return -errno;
		}
		puts(line.line);
		index++;
	}
}

static int list_real_one(const char *kfunc, const char *key)
{
	struct ki_ioc_real real;

	memset(&real, 0, sizeof(real));
	strncpy(real.kfunc, kfunc, sizeof(real.kfunc) - 1);
	strncpy(real.key, key, sizeof(real.key) - 1);

	{
		int feature_ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_GET_REAL);
		if (feature_ret)
			return feature_ret;
	}
	if (ki_ioctl( KI_IOC_GET_REAL_INFO, &real) < 0)
		return -errno;

	printf("%s.%s=%s\n", real.kfunc, real.key, real.value);
	return 0;
}

static int list_real_kfunc(const char *kfunc)
{
	struct ki_ioc_real_key_info info;
	unsigned int index = 0;
	int ret;

	for (;;) {
		memset(&info, 0, sizeof(info));
		info.index = index;
		strncpy(info.kfunc, kfunc, sizeof(info.kfunc) - 1);
		ret = ki_ioctl( KI_IOC_GET_REAL_KEY_LIST, &info);
		if (ret < 0) {
			if (errno == ENOENT && index > 0)
				return 0;
			return -errno;
		}
		ret = list_real_one(info.kfunc, info.key);
		if (ret)
			return ret;
		index++;
	}
}

static int cmd_list(int argc, char **argv)
{
	int fd;
	unsigned int index = 0;
	int ret = 0;
	const char *kfunc = NULL;

	if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") ||
			!strcmp(argv[1], "help"))) {
		fputs(kicmd_help_list, stdout);
		return 0;
	}
	if (require_ki_driver() < 0)
		return 1;

	if (argc >= 2 && !strcmp(argv[1], "process")) {
		if (argc > 3)
			return cli_unexpected_argument("kicmd list process [<pid>]", argv[3]);
		fd = open_ki_checked();
		if (fd < 0)
			return 1;
		ret = cmd_list_process(argc - 1, argv + 1);
		if (ret) {
			fprintf(stderr, "Error: list process: %s\n", strerror(-ret));
			close(fd);
			return 1;
		}
		close(fd);
		debug_log("list process%s%s",
			argc == 3 ? " " : "", argc == 3 ? argv[2] : "");
		return 0;
	}
	if (argc == 2 && (!strcmp(argv[1], "module") || !strcmp(argv[1], "filesystem"))) {
		fd = open_ki_checked();
		if (fd < 0)
			return 1;
		ret = list_kernel_lines(!strcmp(argv[1], "module") ? KI_LIST_MODULE : KI_LIST_FILESYSTEM);
		close(fd);
		return ret ? 1 : 0;
	}
	if (argc > 2)
		return cli_unexpected_argument("kicmd list [<kfunc>]", argv[2]);
	if (argc == 2)
		kfunc = argv[1];

	fd = open_ki_checked();
	if (fd < 0)
		return 1;

	if (kfunc && !strcmp(kfunc, "process")) {
		if (argc > 3) {
			ret = -EINVAL;
		} else {
			ret = cmd_list_process(argc - 1, argv + 1);
		}
	} else if (kfunc) {
		if (argc > 2)
			ret = -EINVAL;
		else
			ret = list_real_kfunc(kfunc);
	} else {
		for (;;) {
			struct ki_ioc_kfunc_info info;

			memset(&info, 0, sizeof(info));
			info.index = index++;
			ret = ki_ioctl( KI_IOC_GET_KFUNC_LIST, &info);
			if (ret < 0) {
				if (errno == ENOENT && index > 0) {
					ret = 0;
					break;
				}
				ret = -errno;
				break;
			}
			if (!(info.features & KI_KFUNC_FEATURE_GET_REAL))
				continue;
			ret = list_real_kfunc(info.kfunc);
			if (ret)
				break;
		}
	}

	if (ret) {
		close(fd);
		return cli_result(ret);
	}

	close(fd);
	debug_log("list %s", kfunc ? kfunc : "all");
	return 0;
}