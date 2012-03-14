/* vi: set sw=4 ts=4:
 *
 * sleep.c - Wait for a number of seconds.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://www.opengroup.org/onlinepubs/009695399/utilities/sleep.html

USE_SLEEP(NEWTOY(sleep, "<1", TOYFLAG_BIN))

config SLEEP
	bool "sleep"
	default y
	help
	  usage: sleep SECONDS

	  Wait before exiting.

config SLEEP_FLOAT
	bool
	default y
	depends on SLEEP && TOYBOX_FLOAT
	help
	  The delay can be a decimal fraction. An optional suffix can be "m"
	  (minutes), "h" (hours), "d" (days), or "s" (seconds, the default).
*/

#include "toys.h"

static unsigned long get_msec(char *param) {
	unsigned long msec;
	char suffix = param[strlen(param) - 1];

	if (CFG_TOYBOX_FLOAT) {
		msec = strtod(*toys.optargs, NULL) * 1000;
	} else {
		msec = strtoul(param, NULL, 10);
	}

	if (!isdigit(suffix)) {
		int imhd[] = { 1, 60, 3600, 86400 };
		char *mhd = "smhd", *c = strchr(mhd, suffix);
		if (!c)
			error_exit("invalid time interval '%s'", param);
		msec *= imhd[c - mhd];
	}

	return msec;
}

void sleep_main(void)
{
	char **arg;
	unsigned long msec = 0;

	for (arg = toys.optargs; *arg; arg++)
		msec += get_msec(*arg);

	struct timespec tv = {
		.tv_sec = msec / 1000,
		.tv_nsec = (msec % 1000) * 1000000
	};

	toys.exitval = nanosleep(&tv, NULL);
}
