#include "../../include/kicmd_internal.h"
#include <stdio.h>
#include <string.h>

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
