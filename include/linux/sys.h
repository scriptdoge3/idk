extern long sys_setup();
extern long sys_exit();
extern long sys_fork();
extern long sys_read();
extern long sys_write();
extern long sys_open();
extern long sys_close();
extern long sys_waitpid();
extern long sys_creat();
extern long sys_link();
extern long sys_unlink();
extern long sys_execve();
extern long sys_chdir();
extern long sys_time();
extern long sys_mknod();
extern long sys_chmod();
extern long sys_chown();
extern long sys_break();
extern long sys_stat();
extern long sys_lseek();
extern long sys_getpid();
extern long sys_mount();
extern long sys_umount();
extern long sys_setuid();
extern long sys_getuid();
extern long sys_stime();
extern long sys_ptrace();
extern long sys_alarm();
extern long sys_fstat();
extern long sys_pause();
extern long sys_utime();
extern long sys_stty();
extern long sys_gtty();
extern long sys_access();
extern long sys_nice();
extern long sys_ftime();
extern long sys_sync();
extern long sys_kill();
extern long sys_rename();
extern long sys_mkdir();
extern long sys_rmdir();
extern long sys_dup();
extern long sys_pipe();
extern long sys_times();
extern long sys_prof();
extern long sys_brk();
extern long sys_setgid();
extern long sys_getgid();
extern long sys_signal();
extern long sys_geteuid();
extern long sys_getegid();
extern long sys_acct();
extern long sys_phys();
extern long sys_lock();
extern long sys_ioctl();
extern long sys_fcntl();
extern long sys_mpx();
extern long sys_setpgid();
extern long sys_ulimit();
extern long sys_uname();
extern long sys_umask();
extern long sys_chroot();
extern long sys_ustat();
extern long sys_dup2();
extern long sys_getppid();
extern long sys_getpgrp();
extern long sys_setsid();
extern long sys_meminfo();

fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read,
sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,
sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,
sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,
sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
sys_getpgrp,sys_setsid,sys_meminfo};
