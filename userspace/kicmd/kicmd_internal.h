#ifndef KICMD_INTERNAL_H
#define KICMD_INTERNAL_H

#include "kicmd_def.h"
#include <stdbool.h>
#include <sys/types.h>
#include <stddef.h>

int cli_missing_argument(const char *, const char *);
int cli_unexpected_argument(const char *, const char *);
int cli_invalid_argument(const char *, const char *);
int cli_unknown_command(const char *, const char *, const char *, const char *const *, size_t);
int cli_result(int);

int require_ki_driver(void);
int module_func(int, char **);
int filesystem_mount(int, char **);
int filesystem_func(int, char **);
int cmd_list_process(int, char **);
int cmd_func_process(int, char **);

bool valid_token(const char *);
bool valid_value(const char *);
int open_ki(void);
int open_ki_checked(void);
void close_ki(void);
int ki_ioctl(unsigned long, void *);
void debug_log(const char *, ...);
int ensure_userd_dir(void);
bool ki_debug_enabled(void);
int check_kfunc_feature(const char *, unsigned int);
int cli_parse_pid(const char *, const char *, const char *, pid_t *);
int cli_parse_u64(const char *, const char *, const char *, unsigned long long *);

void module_report_error(const char *, const char *, int);

bool split_config_line(char *, char **, char **);
int write_config_with_transform(const char *, const char *, const char *,
				const char *, bool, bool);
int cfg_set_active(bool);
int cfg_set(const char *, const char *, const char *);
int cfg_unset(const char *, const char *);
int cfg_reset_kfunc(const char *);
int cfg_reset_all(void);
void cfg_list(const char *);
int cfg_sync(void);
int cfg_set_active_and_ioctl(bool);

int ioctl_value(unsigned long, const char *, const char *, const char *);
int ioctl_key(unsigned long, const char *, const char *);
int ioctl_kfunc(unsigned long, const char *);

int cmd_config(int, char **);
int cmd_list(int, char **);
int cmd_func(int, char **);
int cmd_safemode(int, char **);
int cmd_help(int, char **);
void print_help(void);
int print_version(void);

#endif
