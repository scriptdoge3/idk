/* rm [-rf] file... */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>

static int rflag, fflag;

static int rm(const char * path)
{
	struct stat st;
	DIR * d;
	struct dirent * e;
	char sub[300];
	int status = 0;

	if (stat(path, &st) < 0) {
		if (fflag && errno == ENOENT)
			return 0;
		perror(path);
		return 1;
	}
	if (S_ISDIR(st.st_mode)) {
		if (!rflag) {
			fprintf(stderr, "rm: %s: is a directory\n", path);
			return 1;
		}
		if ((d = opendir(path))) {
			while ((e = readdir(d))) {
				if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
					continue;
				snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
				status |= rm(sub);
			}
			closedir(d);
		}
		if (rmdir(path) < 0) {
			perror(path);
			return 1;
		}
		return status;
	}
	if (unlink(path) < 0) {
		perror(path);
		return 1;
	}
	return 0;
}

int main(int argc, char ** argv)
{
	int i, status = 0;
	char * p;

	for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++)
		for (p = argv[i] + 1; *p; p++)
			if (*p == 'r' || *p == 'R')
				rflag = 1;
			else if (*p == 'f')
				fflag = 1;
	if (i == argc && !fflag) {
		fprintf(stderr, "usage: rm [-rf] file...\n");
		return 2;
	}
	for ( ; i < argc; i++)
		status |= rm(argv[i]);
	return status;
}
