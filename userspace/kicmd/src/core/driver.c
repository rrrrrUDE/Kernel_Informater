#include "../../include/kicmd_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int require_ki_driver(void)
{
	if (open_ki_checked() < 0)
		return -ENODEV;
	return 0;
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
