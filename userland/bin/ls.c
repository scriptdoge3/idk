/* ls [-la] [file...] */
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

static int lflag, aflag, status;

static void mode_str(int m, char * s)
{
	const char * rwx = "rwxrwxrwx";
	int i;

	*s++ = S_ISDIR(m) ? 'd' : S_ISCHR(m) ? 'c' : S_ISBLK(m) ? 'b' :
		S_ISFIFO(m) ? 'p' : '-';
	for (i = 0; i < 9; i++)
		*s++ = (m & (0400 >> i)) ? rwx[i] : '-';
	*s = '\0';
}

static void show(const char * path, const char * name)
{
	struct stat st;
	struct tm * tm;
	char m[11];
	static const char * mon = "JanFebMarAprMayJunJulAugSepOctNovDec";

	if (!lflag) {
		printf("%s\n", name);
		return;
	}
	if (stat(path, &st) < 0) {
		perror(path);
		status = 1;
		return;
	}
	mode_str(st.st_mode, m);
	tm = gmtime(&st.st_mtime);
	if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
		printf("%s %2d %4d %3d, %3d %.3s %2d %4d %s\n", m, st.st_nlink,
			st.st_uid, st.st_rdev >> 8, st.st_rdev & 0xff,
			mon + 3 * tm->tm_mon, tm->tm_mday, 1900 + tm->tm_year, name);
	else
		printf("%s %2d %4d %8ld %.3s %2d %4d %s%s\n", m, st.st_nlink,
			st.st_uid, st.st_size, mon + 3 * tm->tm_mon, tm->tm_mday,
			1900 + tm->tm_year, name, S_ISDIR(st.st_mode) ? "/" : "");
}

static void list(const char * dir, int header)
{
	DIR * d;
	struct dirent * e;
	char ** names = NULL, path[300];
	int n = 0, cap = 0, i, j;

	if (!(d = opendir(dir))) {
		struct stat st;
		if (stat(dir, &st) == 0) {
			show(dir, dir);
			return;
		}
		perror(dir);
		status = 1;
		return;
	}
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.' && !aflag)
			continue;
		if (n == cap) {
			cap = cap ? cap * 2 : 32;
			names = realloc(names, cap * sizeof(char *));
		}
		names[n++] = strdup(e->d_name);
	}
	closedir(d);
	for (i = 1; i < n; i++)
		for (j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
			char * t = names[j]; names[j] = names[j - 1]; names[j - 1] = t;
		}
	if (header)
		printf("%s:\n", dir);
	for (i = 0; i < n; i++) {
		snprintf(path, sizeof path, "%s/%s", dir, names[i]);
		show(path, names[i]);
		free(names[i]);
	}
	free(names);
}

int main(int argc, char ** argv)
{
	int i;
	char * p;

	for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++)
		for (p = argv[i] + 1; *p; p++)
			if (*p == 'l')
				lflag = 1;
			else if (*p == 'a')
				aflag = 1;
			else {
				fprintf(stderr, "usage: ls [-la] [file...]\n");
				return 2;
			}
	if (i == argc)
		list(".", 0);
	for ( ; i < argc; i++)
		list(argv[i], argc - i > 1 || i < argc - 1);
	return status;
}
