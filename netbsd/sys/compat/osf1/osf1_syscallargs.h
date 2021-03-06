/*	$NetBSD: osf1_syscallargs.h,v 1.19 1999/02/09 20:36:17 christos Exp $	*/

/*
 * System call argument lists.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.13 1999/02/09 20:34:17 christos Exp 
 */

#ifndef _OSF1_SYS__SYSCALLARGS_H_
#define _OSF1_SYS__SYSCALLARGS_H_

#ifdef	syscallarg
#undef	syscallarg
#endif

#define	syscallarg(x)								\
		union {								\
			register_t pad;						\
			struct { x datum; } le;					\
			struct {						\
				int8_t pad[ (sizeof (register_t) < sizeof (x))	\
					? 0					\
					: sizeof (register_t) - sizeof (x)];	\
				x datum;					\
			} be;							\
		}

struct osf1_sys_mknod_args {
	syscallarg(char *) path;
	syscallarg(int) mode;
	syscallarg(int) dev;
};

struct osf1_sys_getfsstat_args {
	syscallarg(struct osf1_statfs *) buf;
	syscallarg(long) bufsize;
	syscallarg(int) flags;
};

struct osf1_sys_lseek_args {
	syscallarg(int) fd;
	syscallarg(off_t) offset;
	syscallarg(int) whence;
};

struct osf1_sys_mount_args {
	syscallarg(int) type;
	syscallarg(char *) path;
	syscallarg(int) flags;
	syscallarg(caddr_t) data;
};

struct osf1_sys_unmount_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
};

struct osf1_sys_setuid_args {
	syscallarg(uid_t) uid;
};

struct osf1_sys_open_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct osf1_sys_ioctl_args {
	syscallarg(int) fd;
	syscallarg(int) com;
	syscallarg(caddr_t) data;
};

struct osf1_sys_reboot_args {
	syscallarg(int) opt;
};

struct osf1_sys_execve_args {
	syscallarg(const char *) path;
	syscallarg(char **) argp;
	syscallarg(char **) envp;
};

struct osf1_sys_stat_args {
	syscallarg(char *) path;
	syscallarg(struct osf1_stat *) ub;
};

struct osf1_sys_lstat_args {
	syscallarg(char *) path;
	syscallarg(struct osf1_stat *) ub;
};

struct osf1_sys_mmap_args {
	syscallarg(caddr_t) addr;
	syscallarg(size_t) len;
	syscallarg(int) prot;
	syscallarg(int) flags;
	syscallarg(int) fd;
	syscallarg(off_t) pos;
};

struct osf1_sys_fstat_args {
	syscallarg(int) fd;
	syscallarg(void *) sb;
};

struct osf1_sys_fcntl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(void *) arg;
};

struct osf1_sys_socket_args {
	syscallarg(int) domain;
	syscallarg(int) type;
	syscallarg(int) protocol;
};

struct osf1_sys_readv_args {
	syscallarg(int) fd;
	syscallarg(struct osf1_iovec *) iovp;
	syscallarg(u_int) iovcnt;
};

struct osf1_sys_writev_args {
	syscallarg(int) fd;
	syscallarg(struct osf1_iovec *) iovp;
	syscallarg(u_int) iovcnt;
};

struct osf1_sys_truncate_args {
	syscallarg(char *) path;
	syscallarg(off_t) length;
};

struct osf1_sys_ftruncate_args {
	syscallarg(int) fd;
	syscallarg(off_t) length;
};

struct osf1_sys_setgid_args {
	syscallarg(gid_t) gid;
};

struct osf1_sys_sendto_args {
	syscallarg(int) s;
	syscallarg(caddr_t) buf;
	syscallarg(size_t) len;
	syscallarg(int) flags;
	syscallarg(struct sockaddr *) to;
	syscallarg(int) tolen;
};

struct osf1_sys_getrlimit_args {
	syscallarg(u_int) which;
	syscallarg(struct rlimit *) rlp;
};

struct osf1_sys_setrlimit_args {
	syscallarg(u_int) which;
	syscallarg(struct rlimit *) rlp;
};

struct osf1_sys_sigaction_args {
	syscallarg(int) signum;
	syscallarg(struct osf1_sigaction *) nsa;
	syscallarg(struct osf1_sigaction *) osa;
};

struct osf1_sys_statfs_args {
	syscallarg(char *) path;
	syscallarg(struct osf1_statfs *) buf;
	syscallarg(int) len;
};

struct osf1_sys_fstatfs_args {
	syscallarg(int) fd;
	syscallarg(struct osf1_statfs *) buf;
	syscallarg(int) len;
};

struct osf1_sys_sigaltstack_args {
	syscallarg(struct osf1_sigaltstack *) nss;
	syscallarg(struct osf1_sigaltstack *) oss;
};

struct osf1_sys_usleep_thread_args {
	syscallarg(struct timeval *) sleep;
	syscallarg(struct timeval *) slept;
};

struct osf1_sys_setsysinfo_args {
	syscallarg(u_long) op;
	syscallarg(caddr_t) buffer;
	syscallarg(u_long) nbytes;
	syscallarg(caddr_t) arg;
	syscallarg(u_long) flag;
};

/*
 * System call prototypes.
 */

int	sys_nosys	__P((struct proc *, void *, register_t *));
int	sys_exit	__P((struct proc *, void *, register_t *));
int	sys_fork	__P((struct proc *, void *, register_t *));
int	sys_read	__P((struct proc *, void *, register_t *));
int	sys_write	__P((struct proc *, void *, register_t *));
int	sys_close	__P((struct proc *, void *, register_t *));
int	sys_wait4	__P((struct proc *, void *, register_t *));
int	sys_link	__P((struct proc *, void *, register_t *));
int	sys_unlink	__P((struct proc *, void *, register_t *));
int	sys_chdir	__P((struct proc *, void *, register_t *));
int	sys_fchdir	__P((struct proc *, void *, register_t *));
int	osf1_sys_mknod	__P((struct proc *, void *, register_t *));
int	sys_chmod	__P((struct proc *, void *, register_t *));
int	sys___posix_chown	__P((struct proc *, void *, register_t *));
int	sys_obreak	__P((struct proc *, void *, register_t *));
int	osf1_sys_getfsstat	__P((struct proc *, void *, register_t *));
int	osf1_sys_lseek	__P((struct proc *, void *, register_t *));
int	sys_getpid	__P((struct proc *, void *, register_t *));
int	osf1_sys_mount	__P((struct proc *, void *, register_t *));
int	osf1_sys_unmount	__P((struct proc *, void *, register_t *));
int	osf1_sys_setuid	__P((struct proc *, void *, register_t *));
int	sys_getuid	__P((struct proc *, void *, register_t *));
int	sys_access	__P((struct proc *, void *, register_t *));
int	sys_sync	__P((struct proc *, void *, register_t *));
int	sys_kill	__P((struct proc *, void *, register_t *));
int	sys_setpgid	__P((struct proc *, void *, register_t *));
int	sys_dup	__P((struct proc *, void *, register_t *));
int	sys_pipe	__P((struct proc *, void *, register_t *));
int	osf1_sys_open	__P((struct proc *, void *, register_t *));
int	sys_getgid	__P((struct proc *, void *, register_t *));
int	compat_13_sys_sigprocmask	__P((struct proc *, void *, register_t *));
int	sys___getlogin	__P((struct proc *, void *, register_t *));
int	sys_setlogin	__P((struct proc *, void *, register_t *));
int	sys_acct	__P((struct proc *, void *, register_t *));
int	osf1_sys_ioctl	__P((struct proc *, void *, register_t *));
int	osf1_sys_reboot	__P((struct proc *, void *, register_t *));
int	sys_revoke	__P((struct proc *, void *, register_t *));
int	sys_symlink	__P((struct proc *, void *, register_t *));
int	sys_readlink	__P((struct proc *, void *, register_t *));
int	osf1_sys_execve	__P((struct proc *, void *, register_t *));
int	sys_umask	__P((struct proc *, void *, register_t *));
int	sys_chroot	__P((struct proc *, void *, register_t *));
int	sys_getpgrp	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getpagesize	__P((struct proc *, void *, register_t *));
int	sys_vfork	__P((struct proc *, void *, register_t *));
int	osf1_sys_stat	__P((struct proc *, void *, register_t *));
int	osf1_sys_lstat	__P((struct proc *, void *, register_t *));
int	osf1_sys_mmap	__P((struct proc *, void *, register_t *));
int	sys_munmap	__P((struct proc *, void *, register_t *));
int	osf1_sys_madvise	__P((struct proc *, void *, register_t *));
int	sys_getgroups	__P((struct proc *, void *, register_t *));
int	sys_setgroups	__P((struct proc *, void *, register_t *));
int	sys_setpgid	__P((struct proc *, void *, register_t *));
int	sys_setitimer	__P((struct proc *, void *, register_t *));
int	compat_43_sys_gethostname	__P((struct proc *, void *, register_t *));
int	compat_43_sys_sethostname	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getdtablesize	__P((struct proc *, void *, register_t *));
int	sys_dup2	__P((struct proc *, void *, register_t *));
int	osf1_sys_fstat	__P((struct proc *, void *, register_t *));
int	osf1_sys_fcntl	__P((struct proc *, void *, register_t *));
int	sys_select	__P((struct proc *, void *, register_t *));
int	sys_poll	__P((struct proc *, void *, register_t *));
int	sys_fsync	__P((struct proc *, void *, register_t *));
int	sys_setpriority	__P((struct proc *, void *, register_t *));
int	osf1_sys_socket	__P((struct proc *, void *, register_t *));
int	sys_connect	__P((struct proc *, void *, register_t *));
int	sys_getpriority	__P((struct proc *, void *, register_t *));
int	compat_43_sys_send	__P((struct proc *, void *, register_t *));
int	compat_43_sys_recv	__P((struct proc *, void *, register_t *));
int	compat_13_sys_sigreturn	__P((struct proc *, void *, register_t *));
int	sys_bind	__P((struct proc *, void *, register_t *));
int	sys_setsockopt	__P((struct proc *, void *, register_t *));
int	compat_13_sys_sigsuspend	__P((struct proc *, void *, register_t *));
int	compat_43_sys_sigstack	__P((struct proc *, void *, register_t *));
int	sys_gettimeofday	__P((struct proc *, void *, register_t *));
int	osf1_sys_getrusage	__P((struct proc *, void *, register_t *));
int	sys_getsockopt	__P((struct proc *, void *, register_t *));
int	osf1_sys_readv	__P((struct proc *, void *, register_t *));
int	osf1_sys_writev	__P((struct proc *, void *, register_t *));
int	sys_settimeofday	__P((struct proc *, void *, register_t *));
int	sys___posix_fchown	__P((struct proc *, void *, register_t *));
int	sys_fchmod	__P((struct proc *, void *, register_t *));
int	compat_43_sys_recvfrom	__P((struct proc *, void *, register_t *));
int	sys___posix_rename	__P((struct proc *, void *, register_t *));
int	osf1_sys_truncate	__P((struct proc *, void *, register_t *));
int	osf1_sys_ftruncate	__P((struct proc *, void *, register_t *));
int	osf1_sys_setgid	__P((struct proc *, void *, register_t *));
int	osf1_sys_sendto	__P((struct proc *, void *, register_t *));
int	sys_shutdown	__P((struct proc *, void *, register_t *));
int	sys_mkdir	__P((struct proc *, void *, register_t *));
int	sys_rmdir	__P((struct proc *, void *, register_t *));
int	sys_utimes	__P((struct proc *, void *, register_t *));
int	compat_43_sys_gethostid	__P((struct proc *, void *, register_t *));
int	compat_43_sys_sethostid	__P((struct proc *, void *, register_t *));
int	osf1_sys_getrlimit	__P((struct proc *, void *, register_t *));
int	osf1_sys_setrlimit	__P((struct proc *, void *, register_t *));
int	sys_setsid	__P((struct proc *, void *, register_t *));
int	compat_43_sys_quota	__P((struct proc *, void *, register_t *));
int	osf1_sys_sigaction	__P((struct proc *, void *, register_t *));
int	compat_43_sys_getdirentries	__P((struct proc *, void *, register_t *));
int	osf1_sys_statfs	__P((struct proc *, void *, register_t *));
int	osf1_sys_fstatfs	__P((struct proc *, void *, register_t *));
int	sys___posix_lchown	__P((struct proc *, void *, register_t *));
int	sys_getsid	__P((struct proc *, void *, register_t *));
int	osf1_sys_sigaltstack	__P((struct proc *, void *, register_t *));
int	osf1_sys_usleep_thread	__P((struct proc *, void *, register_t *));
int	osf1_sys_setsysinfo	__P((struct proc *, void *, register_t *));
#endif /* _OSF1_SYS__SYSCALLARGS_H_ */
