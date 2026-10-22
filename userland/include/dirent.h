#ifndef _DIRENT_H
#define _DIRENT_H

/*
 * Linux 0.01 has no readdir system call: directories are read() like
 * files, as an array of Minix v1 entries (2-byte inode, 14-byte name).
 */
#include <sys/types.h>

#define NAME_MAX 14

struct dirent {
	ino_t d_ino;
	char d_name[NAME_MAX+1];
};

typedef struct {
	int fd;
	struct dirent ent;
} DIR;

DIR * opendir(const char * name);
struct dirent * readdir(DIR * d);
int closedir(DIR * d);

#endif
