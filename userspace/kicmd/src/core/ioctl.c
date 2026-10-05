#include "../../include/kicmd_internal.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

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

int ioctl_value(unsigned long request, const char *kfunc, const char *key, const char *value)
{
	struct ki_ioc_value v;
	int fd;
	memset(&v, 0, sizeof(v));
	strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	strncpy(v.key, key, sizeof(v.key) - 1);
	if (value) strncpy(v.value, value, sizeof(v.value) - 1);
	fd = open_ki_checked();
	if (fd < 0) return 1;
	{
		int feature_ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_FUNC);
		if (feature_ret) {
			fprintf(stderr, "Error: kfunc '%s' does not support func: %s\n", kfunc, strerror(-feature_ret));
			close(fd);
			return 1;
		}
	}
	if (ki_ioctl(request, &v) < 0) {
		fprintf(stderr, "Error: ioctl: %s\n", strerror(errno));
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}

int ioctl_key(unsigned long request, const char *kfunc, const char *key)
{
	struct ki_ioc_key v;
	int fd;
	memset(&v, 0, sizeof(v));
	strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	if (key) strncpy(v.key, key, sizeof(v.key) - 1);
	fd = open_ki_checked();
	if (fd < 0) return 1;
	{
		int feature_ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_FUNC);
		if (feature_ret) {
			fprintf(stderr, "Error: kfunc '%s' does not support func: %s\n", kfunc, strerror(-feature_ret));
			close_ki();
			return 1;
		}
	}
	if (ki_ioctl(request, &v) < 0) {
		fprintf(stderr, "Error: ioctl: %s\n", strerror(errno));
		close_ki();
		return 1;
	}
	close_ki();
	return 0;
}

int ioctl_kfunc(unsigned long request, const char *kfunc)
{
	struct ki_ioc_kfunc v;
	int fd;
	memset(&v, 0, sizeof(v));
	if (kfunc) strncpy(v.kfunc, kfunc, sizeof(v.kfunc) - 1);
	fd = open_ki_checked();
	if (fd < 0) return 1;
	if (kfunc && *kfunc) {
		int feature_ret = check_kfunc_feature(kfunc, KI_KFUNC_FEATURE_FUNC);
		if (feature_ret) {
			fprintf(stderr, "Error: kfunc '%s' does not support func: %s\n", kfunc, strerror(-feature_ret));
			close_ki();
			return 1;
		}
	}
	if (ki_ioctl(request, &v) < 0) {
		fprintf(stderr, "Error: ioctl: %s\n", strerror(errno));
		close_ki();
		return 1;
	}
	close_ki();
	return 0;
}
