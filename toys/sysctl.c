/* vi: set sw=4 ts=4:
 *
 * sysctl.c - configure kernel parameters at runtime
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * Not in SUSv4.

USE_SYSCTL(NEWTOY(sysctl, "w:p:nNeqaA", TOYFLAG_SBIN))

config SYSCTL
	bool "sysctl"
	default y
	help
	  usage: sysctl [-aqeNn] [-w variable=value] [-p <file>] [variable]

	  Configure kernel parameters at runtime

	  -a	Show all available parameters and their values.
	  -q	Do not show values that are being set.
	  -e	Ignore errors about unknown keys.
	  -N	Show only parameters.
	  -n	Show only values.
	  -w	Set variable=value.
	  -p	Load variables from <file>. Set to - to read from stdin.
*/

#include "toys.h"

#define FLAG_n 32
#define FLAG_N 16
#define FLAG_e 8
#define FLAG_q 4
#define FLAG_a 2
#define FLAG_A 1

DEFINE_GLOBALS(
	char *file;
	char *var;
	char path[1024];
	char sysctl[1024];
)

#define TT this.sysctl

static char *proc     = "/proc";
static char *proc_sys = "/proc/sys";

static int show_sysctl_value(char *path, char *sysctl) {
	char val_buf[4096];
	char *val = val_buf;
	char *nl;
	int len;

	int f = open(path, O_RDONLY);
	if (f < 0) {
		perror_msg("%s", TT.sysctl);
		return 0;
	}

	len = read(f, val_buf, sizeof(val_buf));
	if (len <= 0) {
		close(f);
		return 0;
	}

	val_buf[len] = '\0';

	while ((nl = strchr(val, '\n'))) {
		nl[0] = '\0';
		if (toys.optflags & FLAG_N) {
			xprintf("%s\n", TT.sysctl);
		} else if (toys.optflags & FLAG_n) {
			xprintf("%s\n", val);
		} else {
			xprintf("%s = %s\n", TT.sysctl, val);
		}
		val = nl + 1; // Move to next line
	}
	close(f);

	return 1;
}

static int prepare_vars(char *prefix, char *path) {
	struct stat st;
	int i, len;

	snprintf(TT.path, sizeof(TT.path) - 1, "%s/%s", prefix, path);
	TT.path[sizeof(TT.path) - 1] = '\0';

	if (stat(TT.path, &st) != 0) {
		if (toys.optflags & FLAG_e && errno != ENOENT)
			perror_msg("%s", path);
		else if ((toys.optflags & FLAG_e) == 0)
			perror_msg("%s", path);
		return 0;
	}

	if (!S_ISREG(st.st_mode))
		return 0;

	strcpy(TT.sysctl, TT.path + strlen(prefix) + 1);
	len = strlen(TT.sysctl);
	for (i = 0; i < len; i++) {
		if (TT.sysctl[i] == '/')
			TT.sysctl[i] = '.';
	}

	return 1;
}

static int write_sysctl_variable(char *variable) {
	char *path = variable;
	char *value = strchr(variable, '=');
	if (!value) {
		error_msg("invalid format: %s", variable);
		return 255;
	}
	value[0] = '\0';
	value++;

	int i, len = strlen(path);
	for (i = 0; i < len; i++) {
		if (path[i] == '.')
			path[i] = '/';
	}

	if (!prepare_vars(proc_sys, path))
		return 255;

	int f = open(TT.path, O_WRONLY);
	if (f < 0) {
		perror_msg("%s", TT.sysctl);
		return 255;
	}
	write(f, value, strlen(value));
	close(f);

	if ((toys.optflags & FLAG_q) == 0) {
		int len = strlen(value);
		while (len && value[--len] == '\n')
			value[len] = '\0';
		xprintf("%s = %s\n", TT.sysctl, value);
	}

	return 0;
}

// Called by dirtree_for_each
static int show_sysctl(char *path) {
	if (prepare_vars(proc, path))
		show_sysctl_value(TT.path, TT.sysctl);
	return 0;
}

void sysctl_main(void)
{
	if (toys.optflags & FLAG_A || toys.optflags & FLAG_a) {
		// Show all sysctl values
		toys.optflags &= ~FLAG_n;
		toys.optflags &= ~FLAG_N;
		dirtree_for_each(proc_sys, show_sysctl);
	} else if (TT.file) {
		int f = 1; // stdin
		char *line;
		if (strcmp(TT.file, "-") != 0) {
			f = xopen(TT.file, O_RDONLY);
		}
		while ((line = get_line(f))) {
			toys.exitval |= write_sysctl_variable(line);
		}
		close(f);
	} else if (TT.var) {
		toys.exitval = write_sysctl_variable(TT.var);
	} else {
		// Show sysctl value(s)
		char **arg;
		for (arg = toys.optargs; *arg; arg++) {
			char *var = *arg;
			int i, len = strlen(var);
			for (i = 0; i < len; i++) {
				if (var[i] == '.')
					var[i] = '/';
			}
			if (prepare_vars(proc_sys, var)) {
				if (!show_sysctl_value(TT.path, TT.sysctl))
					toys.exitval = 255;
			} else {
				if (toys.optflags & FLAG_e) {
					if (errno != ENOENT)
						toys.exitval = 255;
				} else {
					toys.exitval = 255;
				}
			}
		}
	}
}
