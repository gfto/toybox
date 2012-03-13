/* vi: set sw=4 ts=4:
 *
 * mv.c - Move files.
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://www.opengroup.org/onlinepubs/009695399/utilities/mv.html
 *

USE_MV(NEWTOY(mv, "<2fiv", TOYFLAG_BIN))

config MV
	bool "mv"
	default y
	depends on CP && RM
	help
	  usage: mv -fiv SOURCE... DEST

	  Move files from SOURCE to DEST.  If more than one SOURCE, DEST must
	  be a directory.

	  -f	force overwrite
	  -i	interactive, prompt before moving file
	  -v	verbose
*/

#include "toys.h"

#define FLAG_f 4
#define FLAG_i 2
#define FLAG_v 1

// Remove trailing /
// Remove ./ infront
char *normalize(char *x, int xlen) {
	if (!xlen)
		return x;
	while (x[--xlen] == '/')
		x[xlen] = '\0';
	if (xlen > 1) {
		if (x[0] == '.' && x[1] == '/') {
			x += 1;
			while (x[0] == '/')
				x++;
		}
	}
	return x;
}

int internal_exec(char *argv[])
{
	struct toy_list *which;
	struct toy_context c;
	int exitval;
	c = toys;
	toys.optargs = NULL; // Prevent freeing of original args
	which = toy_find(argv[0]);
	if (!which)
		return -1;
	toy_init(which, argv);
	toys.which->toy_main();
	free(toys.optargs);
	exitval = toys.exitval;
	toys = c;
	return exitval;
}

void mv_main(void)
{
	char *dest;
	char *fulldest;
	int fulldest_len = 0;
	struct stat st;
	int i;
	int ret = 0;

	dest = toys.optargs[--toys.optc];

	// Many sources, destination must be a directory
	if (toys.optc > 1) {
		if (stat(dest, &st) != 0 || !S_ISDIR(st.st_mode)) {
			error_exit("'%s' is not a directory", dest);
		}
	}

	fulldest_len = 4096;
	fulldest = xmalloc(fulldest_len);

	dest = normalize(dest, strlen(dest));

	for (i=0; i<toys.optc; i++) {
		char *src = normalize(toys.optargs[i], strlen(toys.optargs[i]));

		if (!strcmp(src, dest))
			continue;

		int flen = strlen(dest) + 1 + strlen(src) + 2;
		if (flen > fulldest_len) {
			fulldest = xrealloc(fulldest, flen);
			fulldest_len = flen;
		}
		snprintf(fulldest, flen - 1, "%s/%s", dest, src);
		fulldest[flen - 1] = '\0';

		if ((toys.optflags & FLAG_i) && !yesno("Confirm mv", 1))
			continue;

		if (toys.optflags & FLAG_v)
			xprintf("mv '%s' -> '%s'\n", src, fulldest);

		if (rename(src, fulldest) != 0) {
			if (errno == EXDEV) {
				char *cp_argv[5] = { "cp", "-af", src, fulldest, NULL };
				char *rm_argv[4] = { "rm", "-rf", src, NULL };
				if (internal_exec(cp_argv) != 0 || internal_exec(rm_argv) != 0)
					toys.exitval = 1;
			} else {
				perror_msg("%s", fulldest);
				toys.exitval = 1;
			}
		}
	}

	if (CFG_TOYBOX_FREE) {
		free(fulldest);
	}

	toys.exitval = ret;
}
