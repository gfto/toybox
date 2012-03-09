/* vi: set sw=4 ts=4:
 *
 * logger.c - log messages.
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://www.opengroup.org/onlinepubs/009695399/utilities/logger.html
 *

USE_LOGGER(NEWTOY(logger, "p:t:f:is", TOYFLAG_BIN))

config LOGGER
	bool "logger"
	default y
	help
	  usage: logger [-is] [-p prio] [-t tag] [-f file] message

	  Logger adds entries to system log.

	  -i	Log program PID along with program name.
	  -s	Log messages to stderr, as well as the system log.
	  -p	Specify priority. The format is facility.level
	  -t	Set program name.
	  -f	Log each line from file.
*/

#include "toys.h"

#include <syslog.h>

#define FLAG_s	1
#define FLAG_i	2

DEFINE_GLOBALS(
	char *file;
	char *tag;
	char *prio;
	int facility;
	int level;
)

#define TT this.logger

struct syslog_txt {
	const char *text;
	int value;
};

static struct syslog_txt syslog_facilities[] = {
	{ "auth",		LOG_AUTH },
	{ "security",	LOG_AUTH },
	{ "authpriv",	LOG_AUTHPRIV },
	{ "cron",		LOG_CRON },
	{ "daemon",		LOG_DAEMON },
	{ "ftp",		LOG_FTP },
	{ "kern",		LOG_KERN },
	{ "local0",		LOG_LOCAL0 },
	{ "local1",		LOG_LOCAL1 },
	{ "local2",		LOG_LOCAL2 },
	{ "local3",		LOG_LOCAL3 },
	{ "local4",		LOG_LOCAL4 },
	{ "local5",		LOG_LOCAL5 },
	{ "local6",		LOG_LOCAL6 },
	{ "local7",		LOG_LOCAL7 },
	{ "lpr",		LOG_LPR },
	{ "mail",		LOG_MAIL },
	{ "news",		LOG_NEWS },
	{ "syslog",		LOG_SYSLOG },
	{ "user",		LOG_USER },
	{ "uucp",		LOG_UUCP },
	{ NULL, 0 },
};

static struct syslog_txt syslog_levels[] = {
	{ "emerg",		LOG_EMERG },
	{ "panic",		LOG_EMERG },
	{ "alert",		LOG_ALERT },
	{ "crit",		LOG_CRIT },
	{ "err",		LOG_ERR },
	{ "error",		LOG_ERR },
	{ "warn",		LOG_WARNING },
	{ "warning",	LOG_WARNING },
	{ "notice",		LOG_NOTICE },
	{ "info",		LOG_INFO },
	{ "debug",		LOG_DEBUG },
	{ NULL, 0 }
};

static void parse_priority() {
	int i;
	struct syslog_txt *opt;
	char *facility_txt = TT.prio;
	char *level_txt = strchr(facility_txt, '.');

	if (level_txt) {
		level_txt[0] = '\0';
		level_txt++;
	}

	for (i=0 ; ; i++) {
		opt = &syslog_facilities[i];
		if (!opt->text) break;
		if (strcmp(facility_txt, opt->text) == 0) {
			TT.facility = opt->value;
			break;
		}
	}

	if (level_txt) {
		for (i=0 ; ; i++) {
			opt = &syslog_levels[i];
			if (!opt->text) break;
			if (strcmp(level_txt, opt->text) == 0) {
				TT.level = opt->value;
				break;
			}
		}
	}
}

void logger_main(void)
{
	char **arg;
	int log_opts = 0;

	TT.facility = LOG_USER;
	TT.level = LOG_INFO;

	if (TT.prio)
		parse_priority();

	if (!TT.tag)
		TT.tag = "logger";

	if (toys.optflags & FLAG_s)
		log_opts |= LOG_PERROR;

	if (toys.optflags & FLAG_i)
		log_opts |= LOG_PID;

	openlog(TT.tag, log_opts, TT.facility);

	if (TT.file || !*toys.optargs) {
		int f = 1; // stdin
		char *line;
		if (TT.file) {
			f = open(TT.file, O_RDONLY);
			if (f < 0)
				perror_exit(TT.file);
		}
		while ((line = get_line(f))) {
			syslog(TT.level, "%s", line);
		}
	} else {
		for (arg = toys.optargs; *arg; arg++) {
			syslog(TT.level, "%s", *arg);
		}
	}

	closelog();
}
