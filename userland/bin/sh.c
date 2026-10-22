/*
 * sh - a small shell for Linux 0.01.
 *
 * Supports: pipelines (|), redirection (< > >>), background jobs (&),
 * lists (; && ||), quoting ('...' "..." \x), $VAR / $? / $$ expansion,
 * * and ? globbing, and the builtins cd, pwd, exit, export, unset, help.
 *
 * The kernel has no getcwd(), so the shell keeps track of the current
 * directory itself.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#define MAXLINE	1024
#define MAXARGS	128
#define MAXCMDS	16

static char cwd[256] = "/";
static int status;			/* $? */

/* ------------------------------------------------------------- lexing */

enum { T_WORD, T_PIPE, T_LT, T_GT, T_GTGT, T_AMP, T_SEMI, T_AND, T_OR, T_END };

struct token {
	int type;
	char * word;
	int glob;			/* unquoted * or ? present */
};

static struct token toks[MAXARGS * 2];
static int ntoks;
static char words[MAXLINE * 4];

static int expand_var(const char ** pp, char ** out, char * end)
{
	const char * p = *pp;
	char name[64], * val, num[16];
	int n = 0;

	if (*p == '?' || *p == '$') {
		snprintf(num, sizeof num, "%d", *p == '?' ? status : getpid());
		val = num;
		p++;
	} else {
		while (n < 63 && (*p == '_' || (*p >= 'a' && *p <= 'z') ||
		       (*p >= 'A' && *p <= 'Z') || (n && *p >= '0' && *p <= '9')))
			name[n++] = *p++;
		name[n] = '\0';
		if (!n) {
			if (*out < end)
				*(*out)++ = '$';
			return 0;
		}
		val = getenv(name);
	}
	*pp = p;
	while (val && *val && *out < end)
		*(*out)++ = *val++;
	return 0;
}

static int lex(const char * p)
{
	char * w = words, * end = words + sizeof words - 2;
	int quoted;

	ntoks = 0;
	for (;;) {
		while (*p == ' ' || *p == '\t' || *p == '\n')
			p++;
		if (ntoks >= MAXARGS * 2 - 1) {
			fprintf(stderr, "sh: line too long\n");
			return -1;
		}
		toks[ntoks].glob = 0;
		toks[ntoks].word = NULL;
		switch (*p) {
		case '\0': case '#':
			toks[ntoks++].type = T_END;
			return 0;
		case '|':
			toks[ntoks++].type = (p[1] == '|') ? T_OR : T_PIPE;
			p += (p[1] == '|') ? 2 : 1;
			continue;
		case '&':
			toks[ntoks++].type = (p[1] == '&') ? T_AND : T_AMP;
			p += (p[1] == '&') ? 2 : 1;
			continue;
		case ';':
			toks[ntoks++].type = T_SEMI;
			p++;
			continue;
		case '<':
			toks[ntoks++].type = T_LT;
			p++;
			continue;
		case '>':
			toks[ntoks++].type = (p[1] == '>') ? T_GTGT : T_GT;
			p += (p[1] == '>') ? 2 : 1;
			continue;
		}
		toks[ntoks].type = T_WORD;
		toks[ntoks].word = w;
		quoted = 0;
		while (*p && !strchr(" \t\n|&;<>", *p) && w < end) {
			if (*p == '\'') {
				for (p++; *p && *p != '\'' && w < end; )
					*w++ = *p++;
				if (*p != '\'') {
					fprintf(stderr, "sh: unterminated '\n");
					return -1;
				}
				p++;
				quoted = 1;
			} else if (*p == '"') {
				for (p++; *p && *p != '"' && w < end; ) {
					if (*p == '\\' && p[1] && strchr("\"\\$", p[1])) {
						*w++ = p[1];
						p += 2;
					} else if (*p == '$') {
						p++;
						expand_var(&p, &w, end);
					} else
						*w++ = *p++;
				}
				if (*p != '"') {
					fprintf(stderr, "sh: unterminated \"\n");
					return -1;
				}
				p++;
				quoted = 1;
			} else if (*p == '\\' && p[1]) {
				*w++ = p[1];
				p += 2;
			} else if (*p == '$') {
				p++;
				expand_var(&p, &w, end);
			} else {
				if (*p == '*' || *p == '?')
					toks[ntoks].glob = 1;
				*w++ = *p++;
			}
		}
		*w++ = '\0';
		/* an unquoted empty expansion disappears, like in a real sh */
		if (!*toks[ntoks].word && !quoted)
			continue;
		ntoks++;
	}
}

/* ------------------------------------------------------------- globbing */

static int match(const char * pat, const char * s)
{
	for ( ; *pat ; pat++, s++) {
		if (*pat == '*') {
			for (pat++; ; s++) {
				if (match(pat, s))
					return 1;
				if (!*s)
					return 0;
			}
		}
		if (!*s || (*pat != '?' && *pat != *s))
			return 0;
	}
	return !*s;
}

static int cmpstr(const void * a, const void * b)
{
	return strcmp(*(char **) a, *(char **) b);
}

static void sort(char ** v, int n)
{
	int i, j;
	char * t;

	for (i = 1; i < n; i++)
		for (j = i; j > 0 && cmpstr(&v[j - 1], &v[j]) > 0; j--) {
			t = v[j]; v[j] = v[j - 1]; v[j - 1] = t;
		}
}

/* expand pattern into argv[*argc...]; patterns only in the last component */
static void glob(char * pat, char ** argv, int * argc)
{
	char dir[256], * slash = strrchr(pat, '/'), * base;
	DIR * d;
	struct dirent * e;
	int start = *argc;

	if (slash) {
		int n = slash - pat;
		if (n >= (int) sizeof dir - 1)
			n = sizeof dir - 2;
		memcpy(dir, pat, n);
		dir[n] = '\0';
		base = slash + 1;
	} else {
		strcpy(dir, ".");
		base = pat;
	}
	if ((d = opendir(slash && !*dir ? "/" : dir))) {
		while ((e = readdir(d)) && *argc < MAXARGS - 1) {
			if (e->d_name[0] == '.' && base[0] != '.')
				continue;
			if (!match(base, e->d_name))
				continue;
			if (slash) {
				char * s = malloc(strlen(dir) + strlen(e->d_name) + 2);
				sprintf(s, "%s/%s", dir, e->d_name);
				argv[(*argc)++] = s;
			} else
				argv[(*argc)++] = strdup(e->d_name);
		}
		closedir(d);
	}
	if (*argc == start)
		argv[(*argc)++] = pat;		/* no match: keep the pattern */
	else
		sort(argv + start, *argc - start);
}

/* ------------------------------------------------------------- commands */

struct cmd {
	char * argv[MAXARGS];
	int argc;
	char * in, * out;
	int append;
};

static void set_cwd(const char * path)
{
	char buf[512], * parts[64], * p;
	int n = 0, i;

	if (path[0] == '/')
		snprintf(buf, sizeof buf, "%s", path);
	else
		snprintf(buf, sizeof buf, "%s/%s", cwd, path);
	for (p = strtok(buf, "/"); p; p = strtok(NULL, "/")) {
		if (!strcmp(p, "."))
			continue;
		if (!strcmp(p, "..")) {
			if (n)
				n--;
			continue;
		}
		if (n < 64)
			parts[n++] = p;
	}
	cwd[0] = '\0';
	for (i = 0; i < n; i++) {
		strcat(cwd, "/");
		strcat(cwd, parts[i]);
	}
	if (!n)
		strcpy(cwd, "/");
}

static void help(void)
{
	printf("Linux 0.01 sh. Builtins: cd pwd exit export unset help\n"
	       "Syntax: cmd args | cmd > file >> file < file & ; && ||\n"
	       "        'quotes' \"$VAR\" $? $$ *.c\n"
	       "Programs live in /bin: ls -l /bin to see them.\n");
}

/* returns -1 if not a builtin, else its exit status */
static int builtin(struct cmd * c)
{
	char * a0 = c->argv[0], * dir, * eq;
	int i;

	if (!strcmp(a0, "cd")) {
		dir = c->argc > 1 ? c->argv[1] : getenv("HOME");
		if (!dir)
			dir = "/";
		if (chdir(dir) < 0) {
			fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
			return 1;
		}
		set_cwd(dir);
		return 0;
	}
	if (!strcmp(a0, "pwd")) {
		printf("%s\n", cwd);
		return 0;
	}
	if (!strcmp(a0, "exit")) {
		exit(c->argc > 1 ? atoi(c->argv[1]) : status);
	}
	if (!strcmp(a0, "help")) {
		help();
		return 0;
	}
	if (!strcmp(a0, "export") || ((eq = strchr(a0, '=')) && eq != a0 && c->argc == 1)) {
		for (i = strcmp(a0, "export") ? 0 : 1; i < c->argc; i++) {
			if (!(eq = strchr(c->argv[i], '=')))
				continue;
			*eq = '\0';
			setenv(c->argv[i], eq + 1, 1);
			*eq = '=';
		}
		return 0;
	}
	if (!strcmp(a0, "unset")) {
		for (i = 1; i < c->argc; i++)
			setenv(c->argv[i], "", 1);
		return 0;
	}
	return -1;
}

static void run_child(struct cmd * c)
{
	char path[256], * dirs, * d, * next;
	int fd;

	if (c->in) {
		if ((fd = open(c->in, O_RDONLY)) < 0) {
			fprintf(stderr, "sh: %s: %s\n", c->in, strerror(errno));
			_exit(1);
		}
		dup2(fd, 0);
		close(fd);
	}
	if (c->out) {
		fd = open(c->out, O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC), 0644);
		if (fd < 0) {
			fprintf(stderr, "sh: %s: %s\n", c->out, strerror(errno));
			_exit(1);
		}
		dup2(fd, 1);
		close(fd);
	}
	if (builtin(c) >= 0) {
		fflush(stdout);
		_exit(0);
	}
	if (strchr(c->argv[0], '/')) {
		execve(c->argv[0], c->argv, environ);
	} else {
		dirs = getenv("PATH");
		dirs = strdup(dirs && *dirs ? dirs : "/bin:/usr/bin");
		for (d = dirs; d; d = next) {
			if ((next = strchr(d, ':')))
				*next++ = '\0';
			snprintf(path, sizeof path, "%s/%s", *d ? d : ".", c->argv[0]);
			execve(path, c->argv, environ);
			if (errno != ENOENT)
				break;
		}
	}
	fprintf(stderr, "sh: %s: %s\n", c->argv[0],
		errno == ENOENT ? "command not found" : strerror(errno));
	_exit(127);
}

static const char * signame(int sig)
{
	static const char * names[] = { "", "Hangup", "", "Quit", "Illegal instruction",
		"Trace trap", "Aborted", "", "Floating point exception", "Killed",
		"User signal 1", "Segmentation fault", "User signal 2", "Broken pipe",
		"Alarm clock", "Terminated" };

	return (sig > 0 && sig < 16) ? names[sig] : "Signal";
}

/* run one pipeline; returns its exit status */
static int run_pipeline(struct cmd * cmds, int n, int bg)
{
	int pids[MAXCMDS], fds[2], in = -1, i, st = 0, r;

	if (n == 1 && !bg && !cmds[0].in && !cmds[0].out &&
	    (r = builtin(&cmds[0])) >= 0)
		return r;
	for (i = 0; i < n; i++) {
		fds[0] = fds[1] = -1;
		if (i < n - 1 && pipe(fds) < 0) {
			perror("sh: pipe");
			break;
		}
		fflush(stdout);
		if ((pids[i] = fork()) < 0) {
			perror("sh: fork");
			break;
		}
		if (!pids[i]) {
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			if (in >= 0) {
				dup2(in, 0);
				close(in);
			}
			if (fds[1] >= 0) {
				dup2(fds[1], 1);
				close(fds[1]);
				close(fds[0]);
			}
			run_child(&cmds[i]);
		}
		if (in >= 0)
			close(in);
		if (fds[1] >= 0)
			close(fds[1]);
		in = fds[0];
	}
	if (in >= 0)
		close(in);
	n = i;
	if (bg) {
		printf("[%d]\n", pids[n - 1]);
		return 0;
	}
	for (i = 0; i < n; i++) {
		while (waitpid(pids[i], &st, 0) < 0 && errno == EINTR)
			;
	}
	if (st & 0x7f) {
		if ((st & 0x7f) != SIGINT)
			fprintf(stderr, "%s\n", signame(st & 0x7f));
		else
			fputc('\n', stderr);
		return 128 + (st & 0x7f);
	}
	return (st >> 8) & 0xff;
}

/* run the tokens of one list element: a pipeline, maybe ending in & */
static void execute(void)
{
	struct cmd cmds[MAXCMDS];
	int t = 0, n = 0, type;

	memset(&cmds[0], 0, sizeof cmds[0]);
	for (;;) {
		type = toks[t].type;
		if (type == T_WORD) {
			if (toks[t].glob)
				glob(toks[t].word, cmds[n].argv, &cmds[n].argc);
			else if (cmds[n].argc < MAXARGS - 1)
				cmds[n].argv[cmds[n].argc++] = toks[t].word;
			t++;
		} else if (type == T_LT || type == T_GT || type == T_GTGT) {
			if (toks[t + 1].type != T_WORD)
				goto syntax;
			if (type == T_LT)
				cmds[n].in = toks[t + 1].word;
			else {
				cmds[n].out = toks[t + 1].word;
				cmds[n].append = (type == T_GTGT);
			}
			t += 2;
		} else if (type == T_PIPE) {
			if (!cmds[n].argc || n == MAXCMDS - 1)
				goto syntax;
			t++;
			n++;
			memset(&cmds[n], 0, sizeof cmds[n]);
		} else
			break;
	}
	if (type == T_AMP && toks[t + 1].type == T_END && cmds[n].argc) {
		status = run_pipeline(cmds, n + 1, 1);
		return;
	}
	if (type != T_END)
		goto syntax;
	if (!cmds[n].argc) {
		if (n)
			goto syntax;
		return;			/* empty command */
	}
	status = run_pipeline(cmds, n + 1, 0);
	return;
syntax:
	fprintf(stderr, "sh: syntax error\n");
	status = 2;
}

/*
 * Split a line at unquoted ; & && || and run the pieces one at a time,
 * so that $? and friends are expanded after the previous one has run.
 */
static void run_line(const char * line)
{
	static char seg[MAXLINE];
	const char * p = line, * start;
	int q, sep, len, skip = 0;

	for (;;) {
		start = p;
		for (q = 0; *p; p++) {
			if (q) {
				if (*p == q)
					q = 0;
				else if (*p == '\\' && q == '"' && p[1])
					p++;
				continue;
			}
			if (*p == '\\' && p[1])
				p++;
			else if (*p == '\'' || *p == '"')
				q = *p;
			else if (*p == ';' || *p == '&' || (*p == '|' && p[1] == '|') ||
			    (*p == '#' && (p == line || p[-1] == ' ' || p[-1] == '\t')))
				break;
		}
		sep = (*p == '&' && p[1] == '&') ? T_AND : (*p == '|') ? T_OR :
			(*p == '&') ? T_AMP : T_SEMI;
		len = p - start + (sep == T_AMP);	/* execute() wants the & */
		if (len >= MAXLINE)
			len = MAXLINE - 1;
		memcpy(seg, start, len);
		seg[len] = '\0';
		if (!skip && lex(seg) == 0)
			execute();
		if (!*p || *p == '#')
			return;
		p += (sep == T_AND || sep == T_OR) ? 2 : 1;
		/* a skipped element leaves $? alone, as in a real sh */
		skip = (sep == T_AND && status) || (sep == T_OR && !status);
	}
}

static void reap(void)
{
	int st, pid;

	while ((pid = waitpid(-1, &st, WNOHANG)) > 0)
		printf("[%d] done (%d)\n", pid, (st >> 8) & 0xff);
}

int main(int argc, char ** argv)
{
	char line[MAXLINE];
	FILE * in = stdin;
	int interactive;
	static FILE script;

	if (!getenv("PATH"))
		setenv("PATH", "/bin:/usr/bin", 1);
	if (!getenv("HOME"))
		setenv("HOME", "/", 1);
	if (argc > 2 && !strcmp(argv[1], "-c")) {
		run_line(argv[2]);
		exit(status);
	}
	if (argc > 1) {
		if ((script.fd = open(argv[1], O_RDONLY)) < 0) {
			perror(argv[1]);
			exit(127);
		}
		in = &script;
	}
	interactive = (in == stdin);
	if (interactive) {
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		/* init starts us as "-sh" in $HOME */
		if (argv[0][0] == '-') {
			char * home = getenv("HOME");
			if (home && chdir(home) == 0)
				set_cwd(home);
			if (access("/etc/motd", 0) == 0) {
				struct cmd c = { { "/bin/cat", "/etc/motd", NULL }, 2 };
				run_pipeline(&c, 1, 0);
			}
		}
	}
	for (;;) {
		if (interactive) {
			reap();
			printf("[%s]# ", cwd);
			fflush(stdout);
		}
		if (!fgets(line, sizeof line, in)) {
			if (interactive)
				printf("exit\n");
			break;
		}
		run_line(line);
	}
	exit(status);
}
