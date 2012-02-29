/* vi: set sw=4 ts=4:
 *
 * chmod.c - Change file mode bits
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://pubs.opengroup.org/onlinepubs/009695399/utilities/chmod.html
 *

USE_CHMOD(NEWTOY(chmod, "<2Rfv", TOYFLAG_BIN))

config CHMOD
	bool "chmod"
	default y
	help
	  usage: chmod [-R] [-f] [-v] mode file...
	  Change mode bits of one or more files.

	  -R	recurse into subdirectories.
	  -f	suppress most error messages.
	  -v	verbose output.
*/

#include "toys.h"

#define FLAG_R 4
#define FLAG_f 2
#define FLAG_v 1

DEFINE_GLOBALS(
	long mode;
)

#define TT this.chmod

static int do_chmod(char *path) {
	int ret = chmod(path, TT.mode);
	if (toys.optflags & FLAG_v)
		xprintf("chmod(%04o, %s)\n", TT.mode, path);
	if (ret == -1 && !(toys.optflags & FLAG_f))
		perror_msg("changing perms of '%s' to %04o", path, TT.mode);
	toys.exitval |= ret;
	return ret;
}

void chmod_main(void)
{
	char **s;
	TT.mode = strtoul(*toys.optargs, NULL, 8);

	for (s=toys.optargs + 1; *s; s++) {
		if (toys.optflags & FLAG_R) {
			dirtree_for_each(*s, do_chmod);
		} else {
			do_chmod(*s);
		}
	}
}
