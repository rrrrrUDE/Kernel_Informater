#include "../../include/kicmd_internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
