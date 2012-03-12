/* vi: set sw=4 ts=4:
 *
 * rm.c - Remove files and/or directories
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://pubs.opengroup.org/onlinepubs/009695399/utilities/rm.html
 *

USE_RM(NEWTOY(rm, "<1fiRrv", TOYFLAG_BIN))

config RM
	bool "rm"
	default y
	help
	  usage: rm [-fiRrv] file...
	  Remove files and/or directories.

	  -f	force remove and do not prompt
	  -i	prompt before every file removal
	  -r -R	remove directories and their contents recursively
	  -v	show what is being done
*/

#define FLAG_f 16
#define FLAG_i 8
#define FLAG_R 4
#define FLAG_r 2
#define FLAG_v 1

#include "toys.h"

#undef __USE_FILE_OFFSET64
#include <fts.h>

static int confirm_rm(char *path)
{
	// Force or no confirmation, just return
	if ((toys.optflags & FLAG_f) || !(toys.optflags & FLAG_i))
		return 1;

	int first, last;

	fprintf(stderr, "remove '%s' (y/N)? ", path);
	fflush(stderr);

	first = last = getchar();
	while (last != '\n' && last != EOF)
		last = getchar();
	return (first == 'y' || first == 'Y');
}

void do_rm(char *file, char *full_path, int is_dir) {
	int ret = is_dir ? rmdir(file) : unlink(file);

	if (ret && !(toys.optflags & FLAG_f)) {
		perror_msg("%s", full_path);
		toys.exitval = 1;
	}

	if (ret == 0 && toys.optflags & FLAG_v)
		xprintf("removed '%s'\n", full_path);
}

static void rm_file(char *file)
{
	struct stat sb;

	if (lstat(file, &sb)) {
		if (!(toys.optflags & FLAG_f)) {
			perror_msg("%s", file);
			toys.exitval = 1;
			return;
		}
	}

	if (!(toys.optflags & FLAG_f) && S_ISDIR(sb.st_mode)) {
		error_msg("%s: is a directory", file);
		toys.exitval = 1;
		return;
	}

	if (!confirm_rm(file))
		return;

	do_rm(file, file, S_ISDIR(sb.st_mode));
}

static void rm_tree(char *path)
{
	FTS *fts;
	FTSENT *ent;
	char *fts_path[2] = { path, NULL };
	struct stat sb;

	fts = fts_open(fts_path, FTS_PHYSICAL | FTS_NOSTAT, NULL);

	if (!fts)
		perror_exit("fts_open");

	while ((ent = fts_read(fts))) {
		if (ent->fts_info == FTS_ERR) { // Error
			perror_exit("%s", ent->fts_path);
			continue;
		}

		if (ent->fts_info == FTS_DNR) { // Can't the read directory
			if (!(toys.optflags & FLAG_f)) {
				perror_msg("%s", ent->fts_path);
				toys.exitval = 1;
			}
			continue;
		}

		if (lstat(ent->fts_accpath, &sb)) {
			if (!(toys.optflags & FLAG_f)) {
				perror_msg("%s", ent->fts_accpath);
				toys.exitval = 1;
				continue;
			}
		}

		// Skip first directory visit
		if (ent->fts_number != 1) {
			ent->fts_number = 1;
			if (S_ISDIR(sb.st_mode))
				continue;
		}

		if (!confirm_rm(ent->fts_accpath))
			continue;

		do_rm(ent->fts_accpath, ent->fts_path, S_ISDIR(sb.st_mode));
	}

	if (errno) {
		perror_msg("fts_read");
		toys.exitval = 1;
	}

	fts_close(fts);
}

void rm_main(void)
{
	char **arg;

	// -R and -r are the same
	if ((toys.optflags & FLAG_R) || (toys.optflags & FLAG_r))
		toys.optflags |= (FLAG_r | FLAG_R);

	for (arg = toys.optargs; *arg; arg++) {
		if (toys.optflags & FLAG_r) {
			rm_tree(*arg);
		} else {
			rm_file(*arg);
		}
	}
}
