/* vi: set ts=4 :*/
/*
 * Generate toybox generated/{Config.in,config.h,globals.h,newtoys.h,help.h}
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

struct strbuf {
	unsigned int len;
	unsigned int nalloc;
	char *data;
};

struct linebuf {
	unsigned int num;
	unsigned int nalloc;
	char **data;
};

struct strbuf config_h;
struct strbuf config_in;
struct strbuf help_h;
struct strbuf globals_h;
struct strbuf globals_union;
struct strbuf build_files;

struct linebuf input_files;
struct linebuf newtoys_h;

int in_config;
int in_help;

int i;
char *line;
size_t linelen;
char tmp[1024];
char *conf, *pos;
FILE *f;

// Library functions
static FILE *xfopen(char *filename, char *mode);

static void linebuf_add(struct linebuf *l, char *text);
static void linebuf_sort(struct linebuf *l);
static void linebuf_free(struct linebuf *l);

static void strbuf_add(struct strbuf *s, char *text);
static void strbuf_printf(struct strbuf *s, const char *fmt, ...);
static void strbuf_add_escaped(struct strbuf *s, char *text);
static void strbuf_dump(struct strbuf *s, char *filename, char *mode);
static void strbuf_free(struct strbuf *s);

/*
 * Parse KConfig Config.in style file and add help entries in help_h
 */
static void parse_help(char *filename) {
	int i, nitems = 0;
	char *config_name = NULL;

	f = xfopen(filename, "r");
	while (getline(&line, &linelen, f) > 0) {
		if (line[0] == '#')
			continue;
		if (strncmp(line, "config ", 7) == 0) {
			in_config = 1;
			in_help = 0;
			free(config_name);
			config_name = strdup(line + 7);
			int config_len = strlen(config_name);
			for (i = 0; i < config_len; i++) {
				config_name[i] = tolower((unsigned char)config_name[i]);
				if (config_name[i] == '\n')
					config_name[i] = '\0';
			}
			// Close already started help line
			if (nitems)
				strbuf_add(&help_h, "\"\n");
			continue;
		}
		if (strncmp(line, "endmenu", 7) == 0) {
			in_config = 0;
			in_help = 0;
		}
		if (!in_config)
			continue;

		if (!in_help) {
			 if (strncmp(line, "\thelp", 5) == 0 || strncmp(line, "    help", 8) == 0) {
				in_help = 1;
				nitems++;
				strbuf_printf(&help_h, "#define help_%s \"", config_name);
			}
			continue;
		} else {
			char *tmpline = line;
			while (isspace(*tmpline))
				tmpline++;
			strbuf_add_escaped(&help_h, tmpline);
			continue;
		}
	}
	strbuf_add(&help_h, "\"\n");

	fclose(f);

	free(config_name);
}

/*
 * Parse Config.in and generate/Config.in and create generated/{help.h}
 * Must be called after generated/Config.in is created from parse_toy_files().
 */
static void generate_config_in() {
	parse_help("Config.in");
	parse_help("generated/Config.in");
	strbuf_dump(&help_h, "generated/help.h", "w");
}

/*
 * Parse input files and create generated/{Config.in,globals.h,newtoys.h}
 */
static void parse_toy_files() {
	int is_toy = 0, in_globals = 0;

	strbuf_add(&globals_union, "extern union global_union {\n");

	// Parse toys
	for (i = 0; i < input_files.num; i++) {
		char *filename = input_files.data[i];
		f = xfopen(filename, "r");

		char *toy = strdup(filename);
		char *toyname = toy;
		pos = strrchr(toyname, '/');
		if (pos)
			toyname = pos + 1; // Remove path
		pos = strstr(toyname, ".c");
		if (pos) // Remove extension
			pos[0] = '\0';

		strbuf_printf(&globals_h, "// %s\n\n", filename);

		while (getline(&line, &linelen, f) > 0) {
			// Parse toys
			if (strncmp(line, "USE_", 4) == 0) {
				is_toy = 1;
				linebuf_add(&newtoys_h, line);
				continue;
			}
			if (!is_toy)
				continue;

			// Parse config
			if (!in_config) {
				if (strncmp(line, "config ", 7) == 0) {
					in_config = 1;
					strbuf_printf(&config_in, "# %s\n", filename);
					strbuf_add(&config_in, line);
					continue;
				}
			} else {
				if (strncmp(line, "*/", 2) == 0) {
					strbuf_add(&config_in, "\n");
					in_config = 0;
					continue;
				} else {
					strbuf_add(&config_in, line);
				}
			}

			// Parse globals
			if (!in_globals) {
				if (strncmp(line, "DEFINE_GLOBALS(", 15) == 0) {
					strbuf_printf(&globals_h, "struct %s_data {\n", toyname);
					strbuf_printf(&globals_union, "\tstruct %s_data %s;\n", toyname, toyname);
					in_globals = 1;
					continue;
				}
			} else {
				if (strncmp(line, ")", 1) == 0) {
					strbuf_add(&globals_h, "};\n");
					in_globals = 0;
					break;
				} else {
					strbuf_add(&globals_h, line);
				}
			}
		}

		free(toy);
		fclose(f);
	}

	strbuf_add(&globals_union, "} this;\n");

	strbuf_dump(&config_in, "generated/Config.in", "w");
	strbuf_dump(&globals_h, "generated/globals.h", "w");
	strbuf_dump(&globals_union, "generated/globals.h", "a");

	// Write newtoys.h
	linebuf_sort(&newtoys_h);
	f = xfopen("generated/newtoys.h", "w");
	const char *first_toy = "NEWTOY(toybox, NULL, 0)\n";
	fwrite(first_toy, strlen(first_toy), 1, f);
	for (i = 0; i < newtoys_h.num; i++) {
		fwrite(newtoys_h.data[i], strlen(newtoys_h.data[i]), 1, f);
	}
	fclose(f);
}

/*
 * Parse .config file and generate generated/config.h and generate/build_files
 */
static void parse_config() {
	// Configu do not exists, create dummy .config
	if (!access(".config", R_OK) == 0) {
		f = xfopen(".config", "w");
		fclose(f);
		return;
	}

	// Parse .config
	f = xfopen(".config", "r");
	while (getline(&line, &linelen, f) > 0) {
		if (strncmp(line, "CONFIG_", 7) == 0) {
			// Config is ON
			conf = line + 7;
			pos = strchr(conf, '=');
			if (pos) pos[0] = '\0';
			strbuf_printf(&config_h, "#define CFG_%s 1\n", conf);
			strbuf_printf(&config_h, "#define USE_%s(...) __VA_ARGS__\n", conf);
			if (strncmp(conf, "TOYBOX_", 7) != 0) {
				// Generate build_files
				int config_len = strlen(conf);
				for (i = 0; i < config_len; i++) {
					conf[i] = tolower((unsigned char)conf[i]);
					if (conf[i] == '\n' || conf[i] == ' ' || conf[i] == '=')
						conf[i] = '\0';
				}
				snprintf(tmp, sizeof(tmp) - 1, "toys/%s.c", conf);
				tmp[sizeof(tmp) - 1] = '\0';
				if (access(tmp, R_OK) == 0)
					strbuf_printf(&build_files, "%s\n", tmp);
			}
			continue;
		}
		if (strncmp(line, "# CONFIG_", 9) == 0) {
			// Config is OFF
			conf = line + 9;
			pos = strchr(conf, ' ');
			if (pos) pos[0] = '\0';
			strbuf_printf(&config_h, "#define CFG_%s 0\n", conf);
			strbuf_printf(&config_h, "#define USE_%s(...)\n", conf);
			continue;
		}
	}
	fclose(f);
	strbuf_dump(&config_h, "generated/config.h", "w");
	strbuf_dump(&build_files, "generated/build_files", "w");
}

/*
 * MAIN
 */
int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s toys/*.c\n", argv[0]);
		exit(1);
	}

	for (i = 1; i < argc; i++) {
		linebuf_add(&input_files, argv[i]);
	}
	linebuf_sort(&input_files);

	parse_toy_files();
	generate_config_in();
	parse_config();

	linebuf_free(&input_files);
	linebuf_free(&newtoys_h);

	strbuf_free(&config_h);
	strbuf_free(&config_in);
	strbuf_free(&help_h);
	strbuf_free(&globals_h);
	strbuf_free(&globals_union);
	strbuf_free(&build_files);

	free(line);

	return 0;
}

/*
 * Library functions
 */
static FILE *xfopen(char *filename, char *mode) {
	FILE *fh = fopen(filename, mode);
	if (!fh) {
		fprintf(stderr, "fopen(%s, %s): %s\n", filename, mode, strerror(errno));
		exit(-1);
	}
	return fh;
}

static void linebuf_add(struct linebuf *l, char *text) {
	int len = strlen(text);
	if (!len)
		return;
	if (!l->data) {
		l->nalloc = 1024;
		l->data = calloc(l->nalloc, sizeof(char *));
	}
	if (l->num + 1 >= l->nalloc) {
		l->nalloc *= 2;
		l->data = realloc(l->data, l->nalloc * sizeof(char *));
		if (!l->data) {
			fprintf(stderr, "realloc(%d): %s\n", l->nalloc * sizeof(char *), strerror(errno));
			exit(-1);
		}
	}
	l->data[l->num] = strdup(text);
	if (l->data[l->num])
		l->num++;
}

static int cmp_string(const void *p1, const void *p2) {
	return strcmp(* (char * const *) p1, * (char * const *) p2);
}

static void linebuf_sort(struct linebuf *l) {
	qsort(l->data, l->num, sizeof(char *), cmp_string);
}

static void linebuf_free(struct linebuf *l) {
	free(l->data);
}

static void strbuf_add(struct strbuf *s, char *text) {
	int len = strlen(text);
	if (!len)
		return;
	if (!s->data) {
		s->nalloc = 1024;
		s->data = calloc(1, s->nalloc);
	}
	s->len += len;
	if (s->len >= s->nalloc) {
		s->nalloc += s->len + 1;
		s->data = realloc(s->data, s->nalloc);
		if (!s->data) {
			fprintf(stderr, "realloc(%d): %s\n", s->nalloc, strerror(errno));
			exit(-1);
		}
	}
	strcat(s->data, text);
}

static void strbuf_printf(struct strbuf *s, const char *fmt, ...) {
	char line[4096];

	va_list args;
	va_start(args, fmt);
	vsnprintf(line, sizeof(line) - 1, fmt, args);
	va_end(args);

	line[sizeof(line) - 1] = '\0';
	strbuf_add(s, line);
}

static void strbuf_add_escaped(struct strbuf *s, char *text) {
	int len = strlen(text), i;
	if (len == 0)
		strbuf_add(s, "\\n");
	for (i = 0; i < len; i++) {
		char c[2] = { text[i], 0 };
		switch (text[i]) {
			case '\n': strbuf_add(s, "\\n"); break;
			case '\t': strbuf_add(s, "    "); break;
			case '\\': strbuf_add(s, "\\\\"); break;
			case '"' : strbuf_add(s, "\\\""); break;
			default  : strbuf_add(s, c); break;
		}
	}
}

static void strbuf_dump(struct strbuf *s, char *filename, char *mode) {
	FILE *fh;
	if (!s->data) {
		unlink(filename);
		return;
	}
	fh = xfopen(filename, mode);
	fwrite(s->data, s->len, 1, fh);
	fclose(fh);
}

static void strbuf_free(struct strbuf *s) {
	free(s->data);
}
