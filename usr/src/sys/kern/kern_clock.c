/*-
 * Copyright (c) 1982, 1986, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)kern_clock.c	7.20 (Berkeley) %G%
 */

#include "param.h"
#include "systm.h"
#include "dkstat.h"
#include "callout.h"
#include "kernel.h"
#include "proc.h"
#include "resourcevar.h"

#include "machine/cpu.h"

#ifdef GPROF
#include "gprof.h"
#endif

#define ADJTIME		/* For now... */
#define	ADJ_TICK 1000
int	adjtimedelta;

/*
 * Clock handling routines.
 *
 * This code is written to operate with two timers which run
 * independently of each other. The main clock, running at hz
 * times per second, is used to do scheduling and timeout calculations.
 * The second timer does resource utilization estimation statistically
 * based on the state of the machine stathz times a second. Both functions
 * can be performed by a single clock (ie hz == stathz), however the 
 * statistics will be much more prone to errors. Ideally a machine
 * would have separate clocks measuring time spent in user state, system
 * state, interrupt state, and idle state. These clocks would allow a non-
 * approximate measure of resource utilization.
 */

/*
 * TODO:
 *	time of day, system/user timing, timeouts, profiling on separate timers
 *	allocate more timeout table slots when table overflows.
 */

/*
 * Bump a timeval by a small number of usec's.
 */
#define BUMPTIME(t, usec) { \
	register struct timeval *tp = (t); \
 \
	tp->tv_usec += (usec); \
	if (tp->tv_usec >= 1000000) { \
		tp->tv_usec -= 1000000; \
		tp->tv_sec++; \
	} \
}

int	ticks;
int	stathz;
int	profhz;
struct	timeval time;
struct	timeval mono_time;
/*
 * The hz hardware interval timer.
 * We update the events relating to real time.
 * If this timer is also being used to gather statistics,
 * we run through the statistics gathering routine as well.
 */
hardclock(frame)
	clockframe frame;
{
	register struct callout *p1;
	register struct proc *p = curproc;
	register struct pstats *pstats;
	register int s;

	/*
	 * Update real-time timeout queue.
	 * At front of queue are some number of events which are ``due''.
	 * The time to these is <= 0 and if negative represents the
	 * number of ticks which have passed since it was supposed to happen.
	 * The rest of the q elements (times > 0) are events yet to happen,
	 * where the time for each is given as a delta from the previous.
	 * Decrementing just the first of these serves to decrement the time
	 * to all events.
	 */
	p1 = calltodo.c_next;
	while (p1) {
		if (--p1->c_time > 0)
			break;
		if (p1->c_time == 0)
			break;
		p1 = p1->c_next;
	}

	/*
	 * Curproc (now in p) is null if no process is running.
	 * We assume that curproc is set in user mode!
	 */
	if (p)
		pstats = p->p_stats;
	/*
	 * Charge the time out based on the mode the cpu is in.
	 * Here again we fudge for the lack of proper interval timers
	 * assuming that the current state has been around at least
	 * one tick.
	 */
		/*
		 * CPU was in user state.  Increment
		 * user time counter, and process process-virtual time
		 * interval timer. 
		 */
		BUMPTIME(&p->p_utime, tick);
		if (timerisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0)
			psignal(p, SIGVTALRM);
	} else {
		/*
		 * CPU was in system state.
		 */
		if (p)
			BUMPTIME(&p->p_stime, tick);
	}

	/*
	 * If the cpu is currently scheduled to a process, then
	 * charge it with resource utilization for a tick, updating
	 * statistics which run in (user+system) virtual time,
	 * such as the cpu time limit and profiling timers.
	 * This assumes that the current process has been running
	 * the entire last tick.
	 */
	if (p) {
		secs = p->p_utime.tv_sec + p->p_stime.tv_sec + 1;
		if (secs > p->p_rlimit[RLIMIT_CPU].rlim_cur) {
			if (secs > p->p_rlimit[RLIMIT_CPU].rlim_max)
				psignal(p, SIGKILL);
			else {
				psignal(p, SIGXCPU);
				if (p->p_rlimit[RLIMIT_CPU].rlim_cur <
				    p->p_rlimit[RLIMIT_CPU].rlim_max)
					p->p_rlimit[RLIMIT_CPU].rlim_cur += 5;
			}
		}
		if (timerisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0)
			psignal(p, SIGPROF);

		/*
		 * We adjust the priority of the current process.
		 * The priority of a process gets worse as it accumulates
		 * CPU time.  The cpu usage estimator (p_cpu) is increased here
		 * and the formula for computing priorities (in kern_synch.c)
		 * will compute a different value each time the p_cpu increases
		 * by 4.  The cpu usage estimator ramps up quite quickly when
		 * the process is running (linearly), and decays away
		 * exponentially, * at a rate which is proportionally slower
		 * when the system is busy.  The basic principal is that the
		 * system will 90% forget that a process used a lot of CPU
		 * time in 5*loadav seconds.  This causes the system to favor
		 * processes which haven't run much recently, and to
		 * round-robin among other processes.
		 */
		p->p_cpticks++;
		if (++p->p_cpu == 0)
			p->p_cpu--;
		if ((p->p_cpu&3) == 0) {
			setpri(p);
			if (p->p_pri >= PUSER)
				p->p_pri = p->p_usrpri;
		}
	}

	/*
	 * If the alternate clock has not made itself known then
	 * we must gather the statistics.
	 */
	if (stathz == 0)
		gatherstats(&frame);

	/*
	 * Increment the time-of-day, and schedule
	 * processing of the callouts at a very low cpu priority,
	 * so we don't keep the relatively high clock interrupt
	 * priority any longer than necessary.
	 */
#ifdef ADJTIME
	if (adjtimedelta == 0)
		bumptime(&time, tick);
	else {
		if (adjtimedelta < 0) {
			bumptime(&time, tick-ADJ_TICK);
			adjtimedelta++;
		} else {
			bumptime(&time, tick+ADJ_TICK);
			adjtimedelta--;
		}
	}
#else
	ticks++;
	if (timedelta == 0) {
		BUMPTIME(&time, tick)
		BUMPTIME(&mono_time, tick)
	} else {
		register delta;

		if (timedelta < 0) {
			delta = tick - tickdelta;
			timedelta += tickdelta;
		} else {
			delta = tick + tickdelta;
			timedelta -= tickdelta;
		}
		BUMPTIME(&time, delta);
		BUMPTIME(&mono_time, delta)
	}
#endif
	setsoftclock();
}

int	dk_ndrive = DK_NDRIVE;
/*
 * Gather statistics on resource utilization.
 *
 * We make a gross assumption: that the system has been in the
 * state it is in (user state, kernel state, interrupt state,
 * or idle state) for the entire last time interval, and
 * update statistics accordingly.
 */
gatherstats(framep)
	clockframe *framep;
{
	register int cpstate, s;

	/*
	 * Determine what state the cpu is in.
	 */
	if (CLKF_USERMODE(framep)) {
		/*
		 * CPU was in user state.
		 */
		if (curproc->p_nice > NZERO)
			cpstate = CP_NICE;
		else
			cpstate = CP_USER;
	} else {
		/*
		 * CPU was in system state.  If profiling kernel
		 * increment a counter.  If no process is running
		 * then this is a system tick if we were running
		 * at a non-zero IPL (in a driver).  If a process is running,
		 * then we charge it with system time even if we were
		 * at a non-zero IPL, since the system often runs
		 * this way during processing of system calls.
		 * This is approximate, but the lack of true interval
		 * timers makes doing anything else difficult.
		 */
		cpstate = CP_SYS;
		if (curproc == NULL && CLKF_BASEPRI(framep))
			cpstate = CP_IDLE;
#ifdef GPROF
		s = CLKF_PC(framep) - s_lowpc;
		if (profiling < 2 && s < s_textsize)
			kcount[s / (HISTFRACTION * sizeof (*kcount))]++;
#endif
	}
	/*
	 * We maintain statistics shown by user-level statistics
	 * programs:  the amount of time in each cpu state, and
	 * the amount of time each of DK_NDRIVE ``drives'' is busy.
	 */
	cp_time[cpstate]++;
	for (s = 0; s < DK_NDRIVE; s++)
		if (dk_busy&(1<<s))
			dk_time[s]++;
}

/*
 * Software priority level clock interrupt.
 * Run periodic events from timeout queue.
 */
/*ARGSUSED*/
softclock(frame)
	clockframe frame;
{

	for (;;) {
		register struct callout *p1;
		register caddr_t arg;
		register int (*func)();
		register int a, s;

		s = splhigh();
		if ((p1 = calltodo.c_next) == 0 || p1->c_time > 0) {
			splx(s);
			break;
		}
		arg = p1->c_arg; func = p1->c_func; a = p1->c_time;
		calltodo.c_next = p1->c_next;
		p1->c_next = callfree;
		callfree = p1;
		splx(s);
		(*func)(arg, a);
	}
	/*
	 * If trapped user-mode and profiling, give it
	 * a profiling tick.
	 */
	if (CLKF_USERMODE(&frame)) {
		register struct proc *p = curproc;

		if (p->p_stats->p_prof.pr_scale)
			profile_tick(p, &frame);
		/*
		 * Check to see if process has accumulated
		 * more than 10 minutes of user time.  If so
		 * reduce priority to give others a chance.
		 */
		if (p->p_ucred->cr_uid && p->p_nice == NZERO &&
		    p->p_utime.tv_sec > 10 * 60) {
			p->p_nice = NZERO + 4;
			setpri(p);
			p->p_pri = p->p_usrpri;
		}
	}
}

/*
 * Arrange that (*func)(arg) is called in t/hz seconds.
 */
timeout(func, arg, t)
	int (*func)();
	caddr_t arg;
	register int t;
{
	register struct callout *p1, *p2, *pnew;
	register int s = splhigh();

	if (t <= 0)
		t = 1;
	pnew = callfree;
	if (pnew == NULL)
		panic("timeout table overflow");
	callfree = pnew->c_next;
	pnew->c_arg = arg;
	pnew->c_func = func;
	for (p1 = &calltodo; (p2 = p1->c_next) && p2->c_time < t; p1 = p2)
		if (p2->c_time > 0)
			t -= p2->c_time;
	p1->c_next = pnew;
	pnew->c_next = p2;
	pnew->c_time = t;
	if (p2)
		p2->c_time -= t;
	splx(s);
}

/*
 * untimeout is called to remove a function timeout call
 * from the callout structure.
 */
untimeout(func, arg)
	int (*func)();
	caddr_t arg;
{
	register struct callout *p1, *p2;
	register int s;

	s = splhigh();
	for (p1 = &calltodo; (p2 = p1->c_next) != 0; p1 = p2) {
		if (p2->c_func == func && p2->c_arg == arg) {
			if (p2->c_next && p2->c_time > 0)
				p2->c_next->c_time += p2->c_time;
			p1->c_next = p2->c_next;
			p2->c_next = callfree;
			callfree = p2;
			break;
		}
	}
	splx(s);
}

/*
 * Compute number of hz until specified time.
 * Used to compute third argument to timeout() from an
 * absolute time.
 */
hzto(tv)
	struct timeval *tv;
{
	register long ticks;
	register long sec;
	int s = splhigh();

	/*
	 * If number of milliseconds will fit in 32 bit arithmetic,
	 * then compute number of milliseconds to time and scale to
	 * ticks.  Otherwise just compute number of hz in time, rounding
	 * times greater than representible to maximum value.
	 *
	 * Delta times less than 25 days can be computed ``exactly''.
	 * Maximum value for any timeout in 10ms ticks is 250 days.
	 */
	sec = tv->tv_sec - time.tv_sec;
	if (sec <= 0x7fffffff / 1000 - 1000)
		ticks = ((tv->tv_sec - time.tv_sec) * 1000 +
			(tv->tv_usec - time.tv_usec) / 1000) / (tick / 1000);
	else if (sec <= 0x7fffffff / hz)
		ticks = sec * hz;
	else
		ticks = 0x7fffffff;
	splx(s);
	return (ticks);
}

/*
 * Return information about system clocks.
 */
/* ARGSUSED */
kinfo_clockrate(op, where, acopysize, arg, aneeded)
	int op;
	register char *where;
	int *acopysize, arg, *aneeded;
{
	int buflen, error;
	struct clockinfo clockinfo;

	*aneeded = sizeof(clockinfo);
	if (where == NULL)
		return (0);
	/*
	 * Check for enough buffering.
	 */
	buflen = *acopysize;
	if (buflen < sizeof(clockinfo)) {
		*acopysize = 0;
		return (0);
	}
	/*
	 * Copyout clockinfo structure.
	 */
	clockinfo.hz = hz;
	clockinfo.stathz = stathz;
	clockinfo.tick = tick;
	clockinfo.profhz = profhz;
	if (error = copyout((caddr_t)&clockinfo, where, sizeof(clockinfo)))
		return (error);
	*acopysize = sizeof(clockinfo);
	return (0);
}
