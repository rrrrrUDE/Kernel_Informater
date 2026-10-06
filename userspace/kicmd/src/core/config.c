#include "../../include/kicmd_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
	char tmp_path[PATH_MAX];
	pid_t pid = getpid();
	unsigned int attempt;
	char line[KICMD_CONFIG_LINE_MAX];
	bool replaced = false;
	int ret;

	ret = ensure_userd_dir();
	if (ret)
		return ret;

	in = fopen(KI_USER_CONFIG, "r");
	if (!in && errno != ENOENT)
		return -errno;

	fd = -1;
	for (attempt = 0; attempt < 100; attempt++) {
		int flags = O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC;

		snprintf(tmp_path, sizeof(tmp_path), "%s/.config.tmp.%ld.%u",
			KICMD_USER_DIR, (long)pid, attempt);
		fd = open(tmp_path, flags, 0600);
		if (fd >= 0)
			break;
		if (errno != EEXIST)
			break;
	}
	if (fd < 0) {
		ret = errno == EEXIST ? -EEXIST : -errno;
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
