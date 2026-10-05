#include "../../include/kicmd_internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
