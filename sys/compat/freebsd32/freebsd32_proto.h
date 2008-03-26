/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $FreeBSD$
 * created from FreeBSD: src/sys/compat/freebsd32/syscalls.master,v 1.99 2008/03/26 15:23:07 dfr Exp 
 */

#ifndef _FREEBSD32_SYSPROTO_H_
#define	_FREEBSD32_SYSPROTO_H_

#include <sys/signal.h>
#include <sys/acl.h>
#include <sys/cpuset.h>
#include <sys/_semaphore.h>
#include <sys/ucontext.h>

#include <bsm/audit_kevents.h>

struct proc;

struct thread;

#define	PAD_(t)	(sizeof(register_t) <= sizeof(t) ? \
		0 : sizeof(register_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define	PADL_(t)	0
#define	PADR_(t)	PAD_(t)
#else
#define	PADL_(t)	PAD_(t)
#define	PADR_(t)	0
#endif

struct freebsd32_wait4_args {
	char pid_l_[PADL_(int)]; int pid; char pid_r_[PADR_(int)];
	char status_l_[PADL_(int *)]; int * status; char status_r_[PADR_(int *)];
	char options_l_[PADL_(int)]; int options; char options_r_[PADR_(int)];
	char rusage_l_[PADL_(struct rusage32 *)]; struct rusage32 * rusage; char rusage_r_[PADR_(struct rusage32 *)];
};
struct freebsd32_recvmsg_args {
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char msg_l_[PADL_(struct msghdr32 *)]; struct msghdr32 * msg; char msg_r_[PADR_(struct msghdr32 *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32_sendmsg_args {
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char msg_l_[PADL_(struct msghdr32 *)]; struct msghdr32 * msg; char msg_r_[PADR_(struct msghdr32 *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32_recvfrom_args {
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char buf_l_[PADL_(u_int32_t)]; u_int32_t buf; char buf_r_[PADR_(u_int32_t)];
	char len_l_[PADL_(u_int32_t)]; u_int32_t len; char len_r_[PADR_(u_int32_t)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char from_l_[PADL_(u_int32_t)]; u_int32_t from; char from_r_[PADR_(u_int32_t)];
	char fromlenaddr_l_[PADL_(u_int32_t)]; u_int32_t fromlenaddr; char fromlenaddr_r_[PADR_(u_int32_t)];
};
struct ofreebsd32_sigpending_args {
	register_t dummy;
};
struct freebsd32_sigaltstack_args {
	char ss_l_[PADL_(struct sigaltstack32 *)]; struct sigaltstack32 * ss; char ss_r_[PADR_(struct sigaltstack32 *)];
	char oss_l_[PADL_(struct sigaltstack32 *)]; struct sigaltstack32 * oss; char oss_r_[PADR_(struct sigaltstack32 *)];
};
struct freebsd32_execve_args {
	char fname_l_[PADL_(char *)]; char * fname; char fname_r_[PADR_(char *)];
	char argv_l_[PADL_(u_int32_t *)]; u_int32_t * argv; char argv_r_[PADR_(u_int32_t *)];
	char envv_l_[PADL_(u_int32_t *)]; u_int32_t * envv; char envv_r_[PADR_(u_int32_t *)];
};
struct freebsd32_setitimer_args {
	char which_l_[PADL_(u_int)]; u_int which; char which_r_[PADR_(u_int)];
	char itv_l_[PADL_(struct itimerval32 *)]; struct itimerval32 * itv; char itv_r_[PADR_(struct itimerval32 *)];
	char oitv_l_[PADL_(struct itimerval32 *)]; struct itimerval32 * oitv; char oitv_r_[PADR_(struct itimerval32 *)];
};
struct freebsd32_getitimer_args {
	char which_l_[PADL_(u_int)]; u_int which; char which_r_[PADR_(u_int)];
	char itv_l_[PADL_(struct itimerval32 *)]; struct itimerval32 * itv; char itv_r_[PADR_(struct itimerval32 *)];
};
struct freebsd32_select_args {
	char nd_l_[PADL_(int)]; int nd; char nd_r_[PADR_(int)];
	char in_l_[PADL_(fd_set *)]; fd_set * in; char in_r_[PADR_(fd_set *)];
	char ou_l_[PADL_(fd_set *)]; fd_set * ou; char ou_r_[PADR_(fd_set *)];
	char ex_l_[PADL_(fd_set *)]; fd_set * ex; char ex_r_[PADR_(fd_set *)];
	char tv_l_[PADL_(struct timeval32 *)]; struct timeval32 * tv; char tv_r_[PADR_(struct timeval32 *)];
};
struct freebsd32_gettimeofday_args {
	char tp_l_[PADL_(struct timeval32 *)]; struct timeval32 * tp; char tp_r_[PADR_(struct timeval32 *)];
	char tzp_l_[PADL_(struct timezone *)]; struct timezone * tzp; char tzp_r_[PADR_(struct timezone *)];
};
struct freebsd32_getrusage_args {
	char who_l_[PADL_(int)]; int who; char who_r_[PADR_(int)];
	char rusage_l_[PADL_(struct rusage32 *)]; struct rusage32 * rusage; char rusage_r_[PADR_(struct rusage32 *)];
};
struct freebsd32_readv_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
};
struct freebsd32_writev_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
};
struct freebsd32_settimeofday_args {
	char tv_l_[PADL_(struct timeval32 *)]; struct timeval32 * tv; char tv_r_[PADR_(struct timeval32 *)];
	char tzp_l_[PADL_(struct timezone *)]; struct timezone * tzp; char tzp_r_[PADR_(struct timezone *)];
};
struct freebsd32_utimes_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char tptr_l_[PADL_(struct timeval32 *)]; struct timeval32 * tptr; char tptr_r_[PADR_(struct timeval32 *)];
};
struct freebsd32_adjtime_args {
	char delta_l_[PADL_(struct timeval32 *)]; struct timeval32 * delta; char delta_r_[PADR_(struct timeval32 *)];
	char olddelta_l_[PADL_(struct timeval32 *)]; struct timeval32 * olddelta; char olddelta_r_[PADR_(struct timeval32 *)];
};
struct freebsd32_semsys_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char a2_l_[PADL_(int)]; int a2; char a2_r_[PADR_(int)];
	char a3_l_[PADL_(int)]; int a3; char a3_r_[PADR_(int)];
	char a4_l_[PADL_(int)]; int a4; char a4_r_[PADR_(int)];
	char a5_l_[PADL_(int)]; int a5; char a5_r_[PADR_(int)];
};
struct freebsd32_msgsys_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char a2_l_[PADL_(int)]; int a2; char a2_r_[PADR_(int)];
	char a3_l_[PADL_(int)]; int a3; char a3_r_[PADR_(int)];
	char a4_l_[PADL_(int)]; int a4; char a4_r_[PADR_(int)];
	char a5_l_[PADL_(int)]; int a5; char a5_r_[PADR_(int)];
	char a6_l_[PADL_(int)]; int a6; char a6_r_[PADR_(int)];
};
struct freebsd32_shmsys_args {
	char which_l_[PADL_(uint32_t)]; uint32_t which; char which_r_[PADR_(uint32_t)];
	char a2_l_[PADL_(uint32_t)]; uint32_t a2; char a2_r_[PADR_(uint32_t)];
	char a3_l_[PADL_(uint32_t)]; uint32_t a3; char a3_r_[PADR_(uint32_t)];
	char a4_l_[PADL_(uint32_t)]; uint32_t a4; char a4_r_[PADR_(uint32_t)];
};
struct freebsd32_stat_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char ub_l_[PADL_(struct stat32 *)]; struct stat32 * ub; char ub_r_[PADR_(struct stat32 *)];
};
struct freebsd32_fstat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char ub_l_[PADL_(struct stat32 *)]; struct stat32 * ub; char ub_r_[PADR_(struct stat32 *)];
};
struct freebsd32_lstat_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char ub_l_[PADL_(struct stat32 *)]; struct stat32 * ub; char ub_r_[PADR_(struct stat32 *)];
};
struct freebsd32_sysctl_args {
	char name_l_[PADL_(int *)]; int * name; char name_r_[PADR_(int *)];
	char namelen_l_[PADL_(u_int)]; u_int namelen; char namelen_r_[PADR_(u_int)];
	char old_l_[PADL_(void *)]; void * old; char old_r_[PADR_(void *)];
	char oldlenp_l_[PADL_(u_int32_t *)]; u_int32_t * oldlenp; char oldlenp_r_[PADR_(u_int32_t *)];
	char new_l_[PADL_(void *)]; void * new; char new_r_[PADR_(void *)];
	char newlen_l_[PADL_(u_int32_t)]; u_int32_t newlen; char newlen_r_[PADR_(u_int32_t)];
};
struct freebsd32_futimes_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char tptr_l_[PADL_(struct timeval32 *)]; struct timeval32 * tptr; char tptr_r_[PADR_(struct timeval32 *)];
};
struct freebsd32_semctl_args {
	char semid_l_[PADL_(int)]; int semid; char semid_r_[PADR_(int)];
	char semnum_l_[PADL_(int)]; int semnum; char semnum_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char arg_l_[PADL_(union semun32 *)]; union semun32 * arg; char arg_r_[PADR_(union semun32 *)];
};
struct freebsd32_msgctl_args {
	char msqid_l_[PADL_(int)]; int msqid; char msqid_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char buf_l_[PADL_(struct msqid_ds32 *)]; struct msqid_ds32 * buf; char buf_r_[PADR_(struct msqid_ds32 *)];
};
struct freebsd32_msgsnd_args {
	char msqid_l_[PADL_(int)]; int msqid; char msqid_r_[PADR_(int)];
	char msgp_l_[PADL_(void *)]; void * msgp; char msgp_r_[PADR_(void *)];
	char msgsz_l_[PADL_(size_t)]; size_t msgsz; char msgsz_r_[PADR_(size_t)];
	char msgflg_l_[PADL_(int)]; int msgflg; char msgflg_r_[PADR_(int)];
};
struct freebsd32_msgrcv_args {
	char msqid_l_[PADL_(int)]; int msqid; char msqid_r_[PADR_(int)];
	char msgp_l_[PADL_(void *)]; void * msgp; char msgp_r_[PADR_(void *)];
	char msgsz_l_[PADL_(size_t)]; size_t msgsz; char msgsz_r_[PADR_(size_t)];
	char msgtyp_l_[PADL_(long)]; long msgtyp; char msgtyp_r_[PADR_(long)];
	char msgflg_l_[PADL_(int)]; int msgflg; char msgflg_r_[PADR_(int)];
};
struct freebsd32_shmctl_args {
	char shmid_l_[PADL_(int)]; int shmid; char shmid_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char buf_l_[PADL_(struct shmid_ds *)]; struct shmid_ds * buf; char buf_r_[PADR_(struct shmid_ds *)];
};
struct freebsd32_clock_gettime_args {
	char clock_id_l_[PADL_(clockid_t)]; clockid_t clock_id; char clock_id_r_[PADR_(clockid_t)];
	char tp_l_[PADL_(struct timespec32 *)]; struct timespec32 * tp; char tp_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_clock_settime_args {
	char clock_id_l_[PADL_(clockid_t)]; clockid_t clock_id; char clock_id_r_[PADR_(clockid_t)];
	char tp_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * tp; char tp_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_clock_getres_args {
	char clock_id_l_[PADL_(clockid_t)]; clockid_t clock_id; char clock_id_r_[PADR_(clockid_t)];
	char tp_l_[PADL_(struct timespec32 *)]; struct timespec32 * tp; char tp_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_nanosleep_args {
	char rqtp_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * rqtp; char rqtp_r_[PADR_(const struct timespec32 *)];
	char rmtp_l_[PADL_(struct timespec32 *)]; struct timespec32 * rmtp; char rmtp_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_lutimes_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char tptr_l_[PADL_(struct timeval32 *)]; struct timeval32 * tptr; char tptr_r_[PADR_(struct timeval32 *)];
};
struct freebsd32_preadv_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
	char offset_l_[PADL_(off_t)]; off_t offset; char offset_r_[PADR_(off_t)];
};
struct freebsd32_pwritev_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
	char offset_l_[PADL_(off_t)]; off_t offset; char offset_r_[PADR_(off_t)];
};
struct freebsd32_modstat_args {
	char modid_l_[PADL_(int)]; int modid; char modid_r_[PADR_(int)];
	char stat_l_[PADL_(struct module_stat32 *)]; struct module_stat32 * stat; char stat_r_[PADR_(struct module_stat32 *)];
};
struct freebsd32_sigtimedwait_args {
	char set_l_[PADL_(const sigset_t *)]; const sigset_t * set; char set_r_[PADR_(const sigset_t *)];
	char info_l_[PADL_(siginfo_t *)]; siginfo_t * info; char info_r_[PADR_(siginfo_t *)];
	char timeout_l_[PADL_(const struct timespec *)]; const struct timespec * timeout; char timeout_r_[PADR_(const struct timespec *)];
};
struct freebsd32_sigwaitinfo_args {
	char set_l_[PADL_(const sigset_t *)]; const sigset_t * set; char set_r_[PADR_(const sigset_t *)];
	char info_l_[PADL_(siginfo_t *)]; siginfo_t * info; char info_r_[PADR_(siginfo_t *)];
};
struct freebsd32_kevent_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char changelist_l_[PADL_(const struct kevent32 *)]; const struct kevent32 * changelist; char changelist_r_[PADR_(const struct kevent32 *)];
	char nchanges_l_[PADL_(int)]; int nchanges; char nchanges_r_[PADR_(int)];
	char eventlist_l_[PADL_(struct kevent32 *)]; struct kevent32 * eventlist; char eventlist_r_[PADR_(struct kevent32 *)];
	char nevents_l_[PADL_(int)]; int nevents; char nevents_r_[PADR_(int)];
	char timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * timeout; char timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_sendfile_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
	char nbytes_l_[PADL_(size_t)]; size_t nbytes; char nbytes_r_[PADR_(size_t)];
	char hdtr_l_[PADL_(struct sf_hdtr32 *)]; struct sf_hdtr32 * hdtr; char hdtr_r_[PADR_(struct sf_hdtr32 *)];
	char sbytes_l_[PADL_(off_t *)]; off_t * sbytes; char sbytes_r_[PADR_(off_t *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32_sigaction_args {
	char sig_l_[PADL_(int)]; int sig; char sig_r_[PADR_(int)];
	char act_l_[PADL_(struct sigaction32 *)]; struct sigaction32 * act; char act_r_[PADR_(struct sigaction32 *)];
	char oact_l_[PADL_(struct sigaction32 *)]; struct sigaction32 * oact; char oact_r_[PADR_(struct sigaction32 *)];
};
struct freebsd32_sigreturn_args {
	char sigcntxp_l_[PADL_(const struct freebsd32_ucontext *)]; const struct freebsd32_ucontext * sigcntxp; char sigcntxp_r_[PADR_(const struct freebsd32_ucontext *)];
};
struct freebsd32_getcontext_args {
	char ucp_l_[PADL_(struct freebsd32_ucontext *)]; struct freebsd32_ucontext * ucp; char ucp_r_[PADR_(struct freebsd32_ucontext *)];
};
struct freebsd32_setcontext_args {
	char ucp_l_[PADL_(const struct freebsd32_ucontext *)]; const struct freebsd32_ucontext * ucp; char ucp_r_[PADR_(const struct freebsd32_ucontext *)];
};
struct freebsd32_swapcontext_args {
	char oucp_l_[PADL_(struct freebsd32_ucontext *)]; struct freebsd32_ucontext * oucp; char oucp_r_[PADR_(struct freebsd32_ucontext *)];
	char ucp_l_[PADL_(const struct freebsd32_ucontext *)]; const struct freebsd32_ucontext * ucp; char ucp_r_[PADR_(const struct freebsd32_ucontext *)];
};
struct freebsd32_umtx_lock_args {
	char umtx_l_[PADL_(struct umtx *)]; struct umtx * umtx; char umtx_r_[PADR_(struct umtx *)];
};
struct freebsd32_umtx_unlock_args {
	char umtx_l_[PADL_(struct umtx *)]; struct umtx * umtx; char umtx_r_[PADR_(struct umtx *)];
};
struct freebsd32_thr_suspend_args {
	char timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * timeout; char timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_umtx_op_args {
	char obj_l_[PADL_(void *)]; void * obj; char obj_r_[PADR_(void *)];
	char op_l_[PADL_(int)]; int op; char op_r_[PADR_(int)];
	char val_l_[PADL_(u_long)]; u_long val; char val_r_[PADR_(u_long)];
	char uaddr_l_[PADL_(void *)]; void * uaddr; char uaddr_r_[PADR_(void *)];
	char uaddr2_l_[PADL_(void *)]; void * uaddr2; char uaddr2_r_[PADR_(void *)];
};
struct freebsd32_thr_new_args {
	char param_l_[PADL_(struct thr_param32 *)]; struct thr_param32 * param; char param_r_[PADR_(struct thr_param32 *)];
	char param_size_l_[PADL_(int)]; int param_size; char param_size_r_[PADR_(int)];
};
struct freebsd32_pread_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(void *)]; void * buf; char buf_r_[PADR_(void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
};
struct freebsd32_pwrite_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(const void *)]; const void * buf; char buf_r_[PADR_(const void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
};
struct freebsd32_mmap_args {
	char addr_l_[PADL_(caddr_t)]; caddr_t addr; char addr_r_[PADR_(caddr_t)];
	char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
	char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char poslo_l_[PADL_(u_int32_t)]; u_int32_t poslo; char poslo_r_[PADR_(u_int32_t)];
	char poshi_l_[PADL_(u_int32_t)]; u_int32_t poshi; char poshi_r_[PADR_(u_int32_t)];
};
struct freebsd32_lseek_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
	char whence_l_[PADL_(int)]; int whence; char whence_r_[PADR_(int)];
};
struct freebsd32_truncate_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char lengthlo_l_[PADL_(u_int32_t)]; u_int32_t lengthlo; char lengthlo_r_[PADR_(u_int32_t)];
	char lengthhi_l_[PADL_(u_int32_t)]; u_int32_t lengthhi; char lengthhi_r_[PADR_(u_int32_t)];
};
struct freebsd32_ftruncate_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char lengthlo_l_[PADL_(u_int32_t)]; u_int32_t lengthlo; char lengthlo_r_[PADR_(u_int32_t)];
	char lengthhi_l_[PADL_(u_int32_t)]; u_int32_t lengthhi; char lengthhi_r_[PADR_(u_int32_t)];
};
int	freebsd32_wait4(struct thread *, struct freebsd32_wait4_args *);
int	freebsd32_recvmsg(struct thread *, struct freebsd32_recvmsg_args *);
int	freebsd32_sendmsg(struct thread *, struct freebsd32_sendmsg_args *);
int	freebsd32_recvfrom(struct thread *, struct freebsd32_recvfrom_args *);
int	freebsd32_sigaltstack(struct thread *, struct freebsd32_sigaltstack_args *);
int	freebsd32_execve(struct thread *, struct freebsd32_execve_args *);
int	freebsd32_setitimer(struct thread *, struct freebsd32_setitimer_args *);
int	freebsd32_getitimer(struct thread *, struct freebsd32_getitimer_args *);
int	freebsd32_select(struct thread *, struct freebsd32_select_args *);
int	freebsd32_gettimeofday(struct thread *, struct freebsd32_gettimeofday_args *);
int	freebsd32_getrusage(struct thread *, struct freebsd32_getrusage_args *);
int	freebsd32_readv(struct thread *, struct freebsd32_readv_args *);
int	freebsd32_writev(struct thread *, struct freebsd32_writev_args *);
int	freebsd32_settimeofday(struct thread *, struct freebsd32_settimeofday_args *);
int	freebsd32_utimes(struct thread *, struct freebsd32_utimes_args *);
int	freebsd32_adjtime(struct thread *, struct freebsd32_adjtime_args *);
int	freebsd32_semsys(struct thread *, struct freebsd32_semsys_args *);
int	freebsd32_msgsys(struct thread *, struct freebsd32_msgsys_args *);
int	freebsd32_shmsys(struct thread *, struct freebsd32_shmsys_args *);
int	freebsd32_stat(struct thread *, struct freebsd32_stat_args *);
int	freebsd32_fstat(struct thread *, struct freebsd32_fstat_args *);
int	freebsd32_lstat(struct thread *, struct freebsd32_lstat_args *);
int	freebsd32_sysctl(struct thread *, struct freebsd32_sysctl_args *);
int	freebsd32_futimes(struct thread *, struct freebsd32_futimes_args *);
int	freebsd32_semctl(struct thread *, struct freebsd32_semctl_args *);
int	freebsd32_msgctl(struct thread *, struct freebsd32_msgctl_args *);
int	freebsd32_msgsnd(struct thread *, struct freebsd32_msgsnd_args *);
int	freebsd32_msgrcv(struct thread *, struct freebsd32_msgrcv_args *);
int	freebsd32_shmctl(struct thread *, struct freebsd32_shmctl_args *);
int	freebsd32_clock_gettime(struct thread *, struct freebsd32_clock_gettime_args *);
int	freebsd32_clock_settime(struct thread *, struct freebsd32_clock_settime_args *);
int	freebsd32_clock_getres(struct thread *, struct freebsd32_clock_getres_args *);
int	freebsd32_nanosleep(struct thread *, struct freebsd32_nanosleep_args *);
int	freebsd32_lutimes(struct thread *, struct freebsd32_lutimes_args *);
int	freebsd32_preadv(struct thread *, struct freebsd32_preadv_args *);
int	freebsd32_pwritev(struct thread *, struct freebsd32_pwritev_args *);
int	freebsd32_modstat(struct thread *, struct freebsd32_modstat_args *);
int	freebsd32_sigtimedwait(struct thread *, struct freebsd32_sigtimedwait_args *);
int	freebsd32_sigwaitinfo(struct thread *, struct freebsd32_sigwaitinfo_args *);
int	freebsd32_kevent(struct thread *, struct freebsd32_kevent_args *);
int	freebsd32_sendfile(struct thread *, struct freebsd32_sendfile_args *);
int	freebsd32_sigaction(struct thread *, struct freebsd32_sigaction_args *);
int	freebsd32_sigreturn(struct thread *, struct freebsd32_sigreturn_args *);
int	freebsd32_getcontext(struct thread *, struct freebsd32_getcontext_args *);
int	freebsd32_setcontext(struct thread *, struct freebsd32_setcontext_args *);
int	freebsd32_swapcontext(struct thread *, struct freebsd32_swapcontext_args *);
int	freebsd32_umtx_lock(struct thread *, struct freebsd32_umtx_lock_args *);
int	freebsd32_umtx_unlock(struct thread *, struct freebsd32_umtx_unlock_args *);
int	freebsd32_thr_suspend(struct thread *, struct freebsd32_thr_suspend_args *);
int	freebsd32_umtx_op(struct thread *, struct freebsd32_umtx_op_args *);
int	freebsd32_thr_new(struct thread *, struct freebsd32_thr_new_args *);
int	freebsd32_pread(struct thread *, struct freebsd32_pread_args *);
int	freebsd32_pwrite(struct thread *, struct freebsd32_pwrite_args *);
int	freebsd32_mmap(struct thread *, struct freebsd32_mmap_args *);
int	freebsd32_lseek(struct thread *, struct freebsd32_lseek_args *);
int	freebsd32_truncate(struct thread *, struct freebsd32_truncate_args *);
int	freebsd32_ftruncate(struct thread *, struct freebsd32_ftruncate_args *);

#ifdef COMPAT_43

struct ofreebsd32_sigaction_args {
	char signum_l_[PADL_(int)]; int signum; char signum_r_[PADR_(int)];
	char nsa_l_[PADL_(struct osigaction32 *)]; struct osigaction32 * nsa; char nsa_r_[PADR_(struct osigaction32 *)];
	char osa_l_[PADL_(struct osigaction32 *)]; struct osigaction32 * osa; char osa_r_[PADR_(struct osigaction32 *)];
};
struct ofreebsd32_sigprocmask_args {
	char how_l_[PADL_(int)]; int how; char how_r_[PADR_(int)];
	char mask_l_[PADL_(osigset_t)]; osigset_t mask; char mask_r_[PADR_(osigset_t)];
};
struct ofreebsd32_sigvec_args {
	char signum_l_[PADL_(int)]; int signum; char signum_r_[PADR_(int)];
	char nsv_l_[PADL_(struct sigvec32 *)]; struct sigvec32 * nsv; char nsv_r_[PADR_(struct sigvec32 *)];
	char osv_l_[PADL_(struct sigvec32 *)]; struct sigvec32 * osv; char osv_r_[PADR_(struct sigvec32 *)];
};
struct ofreebsd32_sigblock_args {
	char mask_l_[PADL_(int)]; int mask; char mask_r_[PADR_(int)];
};
struct ofreebsd32_sigsetmask_args {
	char mask_l_[PADL_(int)]; int mask; char mask_r_[PADR_(int)];
};
struct ofreebsd32_sigsuspend_args {
	char mask_l_[PADL_(int)]; int mask; char mask_r_[PADR_(int)];
};
struct ofreebsd32_sigstack_args {
	char nss_l_[PADL_(struct sigstack32 *)]; struct sigstack32 * nss; char nss_r_[PADR_(struct sigstack32 *)];
	char oss_l_[PADL_(struct sigstack32 *)]; struct sigstack32 * oss; char oss_r_[PADR_(struct sigstack32 *)];
};
int	ofreebsd32_sigaction(struct thread *, struct ofreebsd32_sigaction_args *);
int	ofreebsd32_sigprocmask(struct thread *, struct ofreebsd32_sigprocmask_args *);
int	ofreebsd32_sigpending(struct thread *, struct ofreebsd32_sigpending_args *);
int	ofreebsd32_sigvec(struct thread *, struct ofreebsd32_sigvec_args *);
int	ofreebsd32_sigblock(struct thread *, struct ofreebsd32_sigblock_args *);
int	ofreebsd32_sigsetmask(struct thread *, struct ofreebsd32_sigsetmask_args *);
int	ofreebsd32_sigsuspend(struct thread *, struct ofreebsd32_sigsuspend_args *);
int	ofreebsd32_sigstack(struct thread *, struct ofreebsd32_sigstack_args *);

#endif /* COMPAT_43 */


#ifdef COMPAT_FREEBSD4

struct freebsd4_freebsd32_getfsstat_args {
	char buf_l_[PADL_(struct statfs32 *)]; struct statfs32 * buf; char buf_r_[PADR_(struct statfs32 *)];
	char bufsize_l_[PADL_(long)]; long bufsize; char bufsize_r_[PADR_(long)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd4_freebsd32_statfs_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char buf_l_[PADL_(struct statfs32 *)]; struct statfs32 * buf; char buf_r_[PADR_(struct statfs32 *)];
};
struct freebsd4_freebsd32_fstatfs_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(struct statfs32 *)]; struct statfs32 * buf; char buf_r_[PADR_(struct statfs32 *)];
};
struct freebsd4_freebsd32_fhstatfs_args {
	char u_fhp_l_[PADL_(const struct fhandle *)]; const struct fhandle * u_fhp; char u_fhp_r_[PADR_(const struct fhandle *)];
	char buf_l_[PADL_(struct statfs32 *)]; struct statfs32 * buf; char buf_r_[PADR_(struct statfs32 *)];
};
struct freebsd4_freebsd32_sendfile_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
	char nbytes_l_[PADL_(size_t)]; size_t nbytes; char nbytes_r_[PADR_(size_t)];
	char hdtr_l_[PADL_(struct sf_hdtr32 *)]; struct sf_hdtr32 * hdtr; char hdtr_r_[PADR_(struct sf_hdtr32 *)];
	char sbytes_l_[PADL_(off_t *)]; off_t * sbytes; char sbytes_r_[PADR_(off_t *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd4_freebsd32_sigaction_args {
	char sig_l_[PADL_(int)]; int sig; char sig_r_[PADR_(int)];
	char act_l_[PADL_(struct sigaction32 *)]; struct sigaction32 * act; char act_r_[PADR_(struct sigaction32 *)];
	char oact_l_[PADL_(struct sigaction32 *)]; struct sigaction32 * oact; char oact_r_[PADR_(struct sigaction32 *)];
};
struct freebsd4_freebsd32_sigreturn_args {
	char sigcntxp_l_[PADL_(const struct freebsd4_freebsd32_ucontext *)]; const struct freebsd4_freebsd32_ucontext * sigcntxp; char sigcntxp_r_[PADR_(const struct freebsd4_freebsd32_ucontext *)];
};
int	freebsd4_freebsd32_getfsstat(struct thread *, struct freebsd4_freebsd32_getfsstat_args *);
int	freebsd4_freebsd32_statfs(struct thread *, struct freebsd4_freebsd32_statfs_args *);
int	freebsd4_freebsd32_fstatfs(struct thread *, struct freebsd4_freebsd32_fstatfs_args *);
int	freebsd4_freebsd32_fhstatfs(struct thread *, struct freebsd4_freebsd32_fhstatfs_args *);
int	freebsd4_freebsd32_sendfile(struct thread *, struct freebsd4_freebsd32_sendfile_args *);
int	freebsd4_freebsd32_sigaction(struct thread *, struct freebsd4_freebsd32_sigaction_args *);
int	freebsd4_freebsd32_sigreturn(struct thread *, struct freebsd4_freebsd32_sigreturn_args *);

#endif /* COMPAT_FREEBSD4 */


#ifdef COMPAT_FREEBSD6

struct freebsd6_freebsd32_pread_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(void *)]; void * buf; char buf_r_[PADR_(void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
};
struct freebsd6_freebsd32_pwrite_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(const void *)]; const void * buf; char buf_r_[PADR_(const void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
};
struct freebsd6_freebsd32_mmap_args {
	char addr_l_[PADL_(caddr_t)]; caddr_t addr; char addr_r_[PADR_(caddr_t)];
	char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
	char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char poslo_l_[PADL_(u_int32_t)]; u_int32_t poslo; char poslo_r_[PADR_(u_int32_t)];
	char poshi_l_[PADL_(u_int32_t)]; u_int32_t poshi; char poshi_r_[PADR_(u_int32_t)];
};
struct freebsd6_freebsd32_lseek_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offsetlo_l_[PADL_(u_int32_t)]; u_int32_t offsetlo; char offsetlo_r_[PADR_(u_int32_t)];
	char offsethi_l_[PADL_(u_int32_t)]; u_int32_t offsethi; char offsethi_r_[PADR_(u_int32_t)];
	char whence_l_[PADL_(int)]; int whence; char whence_r_[PADR_(int)];
};
struct freebsd6_freebsd32_truncate_args {
	char path_l_[PADL_(char *)]; char * path; char path_r_[PADR_(char *)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char lengthlo_l_[PADL_(u_int32_t)]; u_int32_t lengthlo; char lengthlo_r_[PADR_(u_int32_t)];
	char lengthhi_l_[PADL_(u_int32_t)]; u_int32_t lengthhi; char lengthhi_r_[PADR_(u_int32_t)];
};
struct freebsd6_freebsd32_ftruncate_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char lengthlo_l_[PADL_(u_int32_t)]; u_int32_t lengthlo; char lengthlo_r_[PADR_(u_int32_t)];
	char lengthhi_l_[PADL_(u_int32_t)]; u_int32_t lengthhi; char lengthhi_r_[PADR_(u_int32_t)];
};
int	freebsd6_freebsd32_pread(struct thread *, struct freebsd6_freebsd32_pread_args *);
int	freebsd6_freebsd32_pwrite(struct thread *, struct freebsd6_freebsd32_pwrite_args *);
int	freebsd6_freebsd32_mmap(struct thread *, struct freebsd6_freebsd32_mmap_args *);
int	freebsd6_freebsd32_lseek(struct thread *, struct freebsd6_freebsd32_lseek_args *);
int	freebsd6_freebsd32_truncate(struct thread *, struct freebsd6_freebsd32_truncate_args *);
int	freebsd6_freebsd32_ftruncate(struct thread *, struct freebsd6_freebsd32_ftruncate_args *);

#endif /* COMPAT_FREEBSD6 */

#define	FREEBSD32_SYS_AUE_freebsd32_wait4	AUE_WAIT4
#define	FREEBSD32_SYS_AUE_freebsd32_recvmsg	AUE_RECVMSG
#define	FREEBSD32_SYS_AUE_freebsd32_sendmsg	AUE_SENDMSG
#define	FREEBSD32_SYS_AUE_freebsd32_recvfrom	AUE_RECVFROM
#define	FREEBSD32_SYS_AUE_freebsd32_sigaltstack	AUE_SIGALTSTACK
#define	FREEBSD32_SYS_AUE_freebsd32_execve	AUE_EXECVE
#define	FREEBSD32_SYS_AUE_freebsd32_setitimer	AUE_SETITIMER
#define	FREEBSD32_SYS_AUE_freebsd32_getitimer	AUE_GETITIMER
#define	FREEBSD32_SYS_AUE_freebsd32_select	AUE_SELECT
#define	FREEBSD32_SYS_AUE_freebsd32_gettimeofday	AUE_GETTIMEOFDAY
#define	FREEBSD32_SYS_AUE_freebsd32_getrusage	AUE_GETRUSAGE
#define	FREEBSD32_SYS_AUE_freebsd32_readv	AUE_READV
#define	FREEBSD32_SYS_AUE_freebsd32_writev	AUE_WRITEV
#define	FREEBSD32_SYS_AUE_freebsd32_settimeofday	AUE_SETTIMEOFDAY
#define	FREEBSD32_SYS_AUE_freebsd32_utimes	AUE_UTIMES
#define	FREEBSD32_SYS_AUE_freebsd32_adjtime	AUE_ADJTIME
#define	FREEBSD32_SYS_AUE_freebsd32_semsys	AUE_SEMSYS
#define	FREEBSD32_SYS_AUE_freebsd32_msgsys	AUE_MSGSYS
#define	FREEBSD32_SYS_AUE_freebsd32_shmsys	AUE_SHMSYS
#define	FREEBSD32_SYS_AUE_freebsd32_stat	AUE_STAT
#define	FREEBSD32_SYS_AUE_freebsd32_fstat	AUE_FSTAT
#define	FREEBSD32_SYS_AUE_freebsd32_lstat	AUE_LSTAT
#define	FREEBSD32_SYS_AUE_freebsd32_sysctl	AUE_SYSCTL
#define	FREEBSD32_SYS_AUE_freebsd32_futimes	AUE_FUTIMES
#define	FREEBSD32_SYS_AUE_freebsd32_semctl	AUE_SEMCTL
#define	FREEBSD32_SYS_AUE_freebsd32_msgctl	AUE_MSGCTL
#define	FREEBSD32_SYS_AUE_freebsd32_msgsnd	AUE_MSGSND
#define	FREEBSD32_SYS_AUE_freebsd32_msgrcv	AUE_MSGRCV
#define	FREEBSD32_SYS_AUE_freebsd32_shmctl	AUE_SHMCTL
#define	FREEBSD32_SYS_AUE_freebsd32_clock_gettime	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_clock_settime	AUE_CLOCK_SETTIME
#define	FREEBSD32_SYS_AUE_freebsd32_clock_getres	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_nanosleep	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_lutimes	AUE_LUTIMES
#define	FREEBSD32_SYS_AUE_freebsd32_preadv	AUE_PREADV
#define	FREEBSD32_SYS_AUE_freebsd32_pwritev	AUE_PWRITEV
#define	FREEBSD32_SYS_AUE_freebsd32_modstat	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_sigtimedwait	AUE_SIGWAIT
#define	FREEBSD32_SYS_AUE_freebsd32_sigwaitinfo	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_kevent	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_sendfile	AUE_SENDFILE
#define	FREEBSD32_SYS_AUE_freebsd32_sigaction	AUE_SIGACTION
#define	FREEBSD32_SYS_AUE_freebsd32_sigreturn	AUE_SIGRETURN
#define	FREEBSD32_SYS_AUE_freebsd32_getcontext	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_setcontext	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_swapcontext	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_umtx_lock	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_umtx_unlock	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_thr_suspend	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_umtx_op	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_thr_new	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_pread	AUE_PREAD
#define	FREEBSD32_SYS_AUE_freebsd32_pwrite	AUE_PWRITE
#define	FREEBSD32_SYS_AUE_freebsd32_mmap	AUE_MMAP
#define	FREEBSD32_SYS_AUE_freebsd32_lseek	AUE_LSEEK
#define	FREEBSD32_SYS_AUE_freebsd32_truncate	AUE_TRUNCATE
#define	FREEBSD32_SYS_AUE_freebsd32_ftruncate	AUE_FTRUNCATE

#undef PAD_
#undef PADL_
#undef PADR_

#endif /* !_FREEBSD32_SYSPROTO_H_ */
