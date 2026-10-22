#include <errno.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

long sys_ftime()
{
	return -ENOSYS;
}

long sys_mknod()
{
	return -ENOSYS;
}

long sys_break()
{
	return -ENOSYS;
}

long sys_mount()
{
	return -ENOSYS;
}

long sys_umount()
{
	return -ENOSYS;
}

long sys_ustat(int dev,struct ustat * ubuf)
{
	return -1;
}

long sys_ptrace()
{
	return -ENOSYS;
}

long sys_stty()
{
	return -ENOSYS;
}

long sys_gtty()
{
	return -ENOSYS;
}

long sys_rename()
{
	return -ENOSYS;
}

long sys_prof()
{
	return -ENOSYS;
}

long sys_setgid(int gid)
{
	if (current->euid && current->uid)
		if (current->gid==gid || current->sgid==gid)
			current->egid=gid;
		else
			return -EPERM;
	else
		current->gid=current->egid=gid;
	return 0;
}

long sys_acct()
{
	return -ENOSYS;
}

long sys_phys()
{
	return -ENOSYS;
}

long sys_lock()
{
	return -ENOSYS;
}

long sys_mpx()
{
	return -ENOSYS;
}

long sys_ulimit()
{
	return -ENOSYS;
}

long sys_time(long * tloc)
{
	long i;

	i = CURRENT_TIME;
	if (tloc) {
		verify_area(tloc,sizeof(long));
		put_fs_long(i,(unsigned long *)tloc);
	}
	return i;
}

long sys_setuid(int uid)
{
	if (current->euid && current->uid)
		if (uid==current->uid || current->suid==current->uid)
			current->euid=uid;
		else
			return -EPERM;
	else
		current->euid=current->uid=uid;
	return 0;
}

long sys_stime(long * tptr)
{
	if (current->euid && current->uid)
		return -1;
	startup_time = get_fs_long((unsigned long *)tptr) - jiffies/HZ;
	return 0;
}

long sys_times(struct tms * tbuf)
{
	if (!tbuf)
		return jiffies;
	verify_area(tbuf,sizeof *tbuf);
	put_fs_long(current->utime,(unsigned long *)&tbuf->tms_utime);
	put_fs_long(current->stime,(unsigned long *)&tbuf->tms_stime);
	put_fs_long(current->cutime,(unsigned long *)&tbuf->tms_cutime);
	put_fs_long(current->cstime,(unsigned long *)&tbuf->tms_cstime);
	return jiffies;
}

long sys_brk(unsigned long end_data_seg)
{
	if (end_data_seg >= current->end_code &&
	    end_data_seg < current->start_stack - 16384)
		current->brk = end_data_seg;
	return current->brk;
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
long sys_setpgid(int pid, int pgid)
{
	int i;

	if (!pid)
		pid = current->pid;
	if (!pgid)
		pgid = pid;
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] && task[i]->pid==pid) {
			if (task[i]->leader)
				return -EPERM;
			if (task[i]->session != current->session)
				return -EPERM;
			task[i]->pgrp = pgid;
			return 0;
		}
	return -ESRCH;
}

long sys_getpgrp(void)
{
	return current->pgrp;
}

long sys_setsid(void)
{
	if (current->uid && current->euid)
		return -EPERM;
	if (current->leader)
		return -EPERM;
	current->leader = 1;
	current->session = current->pgrp = current->pid;
	current->tty = -1;
	return current->pgrp;
}

long sys_uname(struct utsname * name)
{
	static struct utsname thisname = {
		"linux .0","nodename","release ","version ","x86_64"
	};
	int i;

	if (!name) return -1;
	verify_area(name,sizeof *name);
	for(i=0;i<sizeof *name;i++)
		put_fs_byte(((char *) &thisname)[i],i+(char *) name);
	return (0);
}

long sys_umask(int mask)
{
	int old = current->umask;

	current->umask = mask & 0777;
	return (old);
}
