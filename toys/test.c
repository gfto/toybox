/* vi: set sw=4 ts=4:
 *
 * test.c - evaluate expression
 *
 * test(1); version 7-like  --  author Erik Baalbergen
 * modified by Eric Gisin to be used as built-in.
 * modified by Arnold Robbins to add SVR3 compatibility
 * (-x -c -b -p -u -g -k) plus Korn's -L -nt -ot -ef and new -S (socket).
 * modified by J.T. Conklin for NetBSD.
 *
 * This program is in the Public Domain.
 *
 * See http://www.opengroup.org/onlinepubs/009695399/utilities/test.html
 *
 * TODO: Add [ command alias

USE_TEST(NEWTOY(test, "?", TOYFLAG_BIN))

config TEST
    bool "test"
    default y
    help
      usage: test EXPRESSION

      Exit with the status determined by EXPRESSION.

      See man 1 test for allowed EXPRESSIONs.
*/

#include "toys.h"

/* test(1) accepts the following grammar:
	expr_or		::= expr_and | expr_and "-o" expr_or ;
	expr_and	::= expr_not | expr_not "-a" expr_and ;
	expr_not	::= primary | "!" primary
	primary		::= unary-operator operand
		| operand binary-operator operand
		| operand
		| "(" expr_or ")"
		;
	unary-operator ::= "-r"|"-w"|"-x"|"-f"|"-d"|"-c"|"-b"|"-p"|
		"-u"|"-g"|"-k"|"-s"|"-t"|"-z"|"-n"|"-o"|"-O"|"-G"|"-L"|"-S";

	binary-operator ::= "="|"!="|"-eq"|"-ne"|"-ge"|"-gt"|"-le"|"-lt"|
			"-nt"|"-ot"|"-ef";
	operand ::= <any legal UNIX file name>
*/

enum token {
	EOI,
	FILRD,
	FILWR,
	FILEX,
	FILEXIST,
	FILREG,
	FILDIR,
	FILCDEV,
	FILBDEV,
	FILFIFO,
	FILSOCK,
	FILSYM,
	FILGZ,
	FILTT,
	FILSUID,
	FILSGID,
	FILSTCK,
	FILNT,
	FILOT,
	FILEQ,
	FILUID,
	FILGID,
	STREZ,
	STRNZ,
	STREQ,
	STRNE,
	STRLT,
	STRGT,
	INTEQ,
	INTNE,
	INTGE,
	INTGT,
	INTLE,
	INTLT,
	UNOT,
	BAND,
	BOR,
	LPAREN,
	RPAREN,
	OPERAND
};

enum token_types {
	UNOP,
	BINOP,
	BUNOP,
	BBINOP,
	PAREN
};

static struct t_op {
	const char *op_text;
	short op_num, op_type;
} const ops [] = {
	{"-r",	FILRD,	UNOP},
	{"-w",	FILWR,	UNOP},
	{"-x",	FILEX,	UNOP},
	{"-e",	FILEXIST,UNOP},
	{"-f",	FILREG,	UNOP},
	{"-d",	FILDIR,	UNOP},
	{"-c",	FILCDEV,UNOP},
	{"-b",	FILBDEV,UNOP},
	{"-p",	FILFIFO,UNOP},
	{"-u",	FILSUID,UNOP},
	{"-g",	FILSGID,UNOP},
	{"-k",	FILSTCK,UNOP},
	{"-s",	FILGZ,	UNOP},
	{"-t",	FILTT,	UNOP},
	{"-z",	STREZ,	UNOP},
	{"-n",	STRNZ,	UNOP},
	{"-h",	FILSYM,	UNOP},		/* for backwards compat */
	{"-O",	FILUID,	UNOP},
	{"-G",	FILGID,	UNOP},
	{"-L",	FILSYM,	UNOP},
	{"-S",	FILSOCK,UNOP},
	{"=",	STREQ,	BINOP},
	{"!=",	STRNE,	BINOP},
	{"<",	STRLT,	BINOP},
	{">",	STRGT,	BINOP},
	{"-eq",	INTEQ,	BINOP},
	{"-ne",	INTNE,	BINOP},
	{"-ge",	INTGE,	BINOP},
	{"-gt",	INTGT,	BINOP},
	{"-le",	INTLE,	BINOP},
	{"-lt",	INTLT,	BINOP},
	{"-nt",	FILNT,	BINOP},
	{"-ot",	FILOT,	BINOP},
	{"-ef",	FILEQ,	BINOP},
	{"!",	UNOT,	BUNOP},
	{"-a",	BAND,	BBINOP},
	{"-o",	BOR,	BBINOP},
	{"(",	LPAREN,	PAREN},
	{")",	RPAREN,	PAREN},
	{0,	0,	0}
};

static char **t_wp;
static struct t_op const *t_wp_op;

static void syntax(const char *, const char *);
static int expr_or(enum token);
static int expr_and(enum token);
static int expr_not(enum token);
static int primary(enum token);
static int binop(void);
static int file_stat(char *, enum token);
static enum token t_lex(char *);
static int is_operand(void);
static int getn(const char *);
static int newerf(const char *, const char *);
static int olderf(const char *, const char *);
static int equalf(const char *, const char *);

static void syntax(const char *op, const char *msg)
{
	if (op && *op)
		error_exit("%s: %s", op, msg);
	else
		error_exit("%s", msg);
}

static int expr_or(enum token n)
{
	int res;
	res = expr_and(n);
	if (t_lex(*++t_wp) == BOR)
		return expr_or(t_lex(*++t_wp)) || res;
	t_wp--;
	return res;
}

static int expr_and(enum token n)
{
	int res;
	res = expr_not(n);
	if (t_lex(*++t_wp) == BAND)
		return expr_and(t_lex(*++t_wp)) && res;
	t_wp--;
	return res;
}

static int expr_not(enum token n)
{
	if (n == UNOT)
		return !expr_not(t_lex(*++t_wp));
	return primary(n);
}

static int primary(enum token n)
{
	enum token nn;
	int res;

	if (n == EOI)
		return 0;		/* missing expression */
	if (n == LPAREN) {
		if ((nn = t_lex(*++t_wp)) == RPAREN)
			return 0;	/* missing expression */
		res = expr_or(nn);
		if (t_lex(*++t_wp) != RPAREN)
			syntax(NULL, "closing paren expected");
		return res;
	}
	if (t_wp_op && t_wp_op->op_type == UNOP) {
		/* unary expression */
		if (*++t_wp == NULL)
			syntax(t_wp_op->op_text, "argument expected");
		switch (n) {
		case STREZ:
			return strlen(*t_wp) == 0;
		case STRNZ:
			return strlen(*t_wp) != 0;
		case FILTT:
			return isatty(getn(*t_wp));
		default:
			return file_stat(*t_wp, n);
		}
	}

	if (t_lex(t_wp[1]), t_wp_op && t_wp_op->op_type == BINOP) {
		return binop();
	}

	return strlen(*t_wp) > 0;
}

static int binop(void)
{
	const char *opnd1, *opnd2;
	struct t_op const *op;

	opnd1 = *t_wp;
	(void) t_lex(*++t_wp);
	op = t_wp_op;

	if ((opnd2 = *++t_wp) == NULL)
		syntax(op->op_text, "argument expected");

	switch (op->op_num) {
	case STREQ:	return strcmp(opnd1, opnd2) == 0;
	case STRNE:	return strcmp(opnd1, opnd2) != 0;
	case STRLT:	return strcmp(opnd1, opnd2) < 0;
	case STRGT:	return strcmp(opnd1, opnd2) > 0;
	case INTEQ:	return getn(opnd1) == getn(opnd2);
	case INTNE:	return getn(opnd1) != getn(opnd2);
	case INTGE:	return getn(opnd1) >= getn(opnd2);
	case INTGT:	return getn(opnd1) > getn(opnd2);
	case INTLE:	return getn(opnd1) <= getn(opnd2);
	case INTLT:	return getn(opnd1) < getn(opnd2);
	case FILNT:	return newerf(opnd1, opnd2);
	case FILOT:	return olderf(opnd1, opnd2);
	case FILEQ:	return equalf(opnd1, opnd2);
	default: abort();
	}
}

static int file_stat(char *nm, enum token mode)
{
	struct stat s;

	if (mode == FILSYM ? lstat(nm, &s) : stat(nm, &s))
		return 0;

	switch (mode) {
	case FILRD:		return access(nm, R_OK) == 0;
	case FILWR:		return access(nm, W_OK) == 0;
	case FILEX:		return access(nm, X_OK) == 0;
	case FILEXIST:	return access(nm, F_OK) == 0;
	case FILREG:	return S_ISREG(s.st_mode);
	case FILDIR:	return S_ISDIR(s.st_mode);
	case FILCDEV:	return S_ISCHR(s.st_mode);
	case FILBDEV:	return S_ISBLK(s.st_mode);
	case FILFIFO:	return S_ISFIFO(s.st_mode);
	case FILSOCK:	return S_ISSOCK(s.st_mode);
	case FILSYM:	return S_ISLNK(s.st_mode);
	case FILSUID:	return (s.st_mode & S_ISUID) != 0;
	case FILSGID:	return (s.st_mode & S_ISGID) != 0;
	case FILSTCK:	return (s.st_mode & S_ISVTX) != 0;
	case FILGZ:		return s.st_size > (off_t)0;
	case FILUID:	return s.st_uid == geteuid();
	case FILGID:	return s.st_gid == getegid();
	default:
		return 1;
	}
}

static enum token t_lex(char *s)
{
	struct t_op const *op = ops;
	if (s == 0) {
		t_wp_op = NULL;
		return EOI;
	}
	while (op->op_text) {
		if (strcmp(s, op->op_text) == 0) {
			if ((op->op_type == UNOP && is_operand()) ||
			    (op->op_num == LPAREN && *(t_wp+1) == 0))
				break;
			t_wp_op = op;
			return op->op_num;
		}
		op++;
	}
	t_wp_op = NULL;
	return OPERAND;
}

static int is_operand(void)
{
	struct t_op const *op;
	char *s, *t;

	op = ops;
	if ((s  = *(t_wp+1)) == 0)
		return 1;
	if ((t = *(t_wp+2)) == 0)
		return 0;
	while (op->op_text) {
		if (strcmp(s, op->op_text) == 0)
			return op->op_type == BINOP &&
			    (t[0] != ')' || t[1] != '\0'); 
		op++;
	}
	return 0;
}

/* atoi with error_exit detection */
static int getn(const char *s)
{
	char *p;
	long r;

	errno = 0;
	r = strtol(s, &p, 10);

	if (errno != 0)
		error_exit("%s: out of range", s);

	while (isspace((unsigned char)*p))
		p++;

	if (*p)
		error_exit("%s: bad number", s);

	return (int) r;
}

static int newerf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
		stat(f2, &b2) == 0 &&
		b1.st_mtime > b2.st_mtime);
}

static int olderf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
		stat(f2, &b2) == 0 &&
		b1.st_mtime < b2.st_mtime);
}

static int equalf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
		stat(f2, &b2) == 0 &&
		b1.st_dev == b2.st_dev &&
		b1.st_ino == b2.st_ino);
}

void test_main(void) {
	int res;

	t_wp = toys.optargs;
	res = !expr_or(t_lex(*t_wp));

	if (*t_wp != NULL && *++t_wp != NULL)
		syntax(*t_wp, "unexpected operator");

	toys.exitval = res;
}
