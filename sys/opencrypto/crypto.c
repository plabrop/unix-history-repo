/*	$FreeBSD$	*/
/*	$OpenBSD: crypto.c,v 1.38 2002/06/11 11:14:29 beck Exp $	*/
/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000, 2001 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <vm/uma.h>
#include <opencrypto/cryptodev.h>

#define	SESID2HID(sid)	(((sid) >> 32) & 0xffffffff)

/*
 * Crypto drivers register themselves by allocating a slot in the
 * crypto_drivers table with crypto_get_driverid() and then registering
 * each algorithm they support with crypto_register() and crypto_kregister().
 */
static	struct mtx crypto_drivers_mtx;		/* lock on driver table */
#define	CRYPTO_DRIVER_LOCK()	mtx_lock(&crypto_drivers_mtx)
#define	CRYPTO_DRIVER_UNLOCK()	mtx_unlock(&crypto_drivers_mtx)
static	struct cryptocap *crypto_drivers = NULL;
static	int crypto_drivers_num = 0;

/*
 * There are two queues for crypto requests; one for symmetric (e.g.
 * cipher) operations and one for asymmetric (e.g. MOD)operations.
 * A single mutex is used to lock access to both queues.  We could
 * have one per-queue but having one simplifies handling of block/unblock
 * operations.
 */
static	TAILQ_HEAD(,cryptop) crp_q;		/* request queues */
static	TAILQ_HEAD(,cryptkop) crp_kq;
static	struct mtx crypto_q_mtx;
#define	CRYPTO_Q_LOCK()		mtx_lock(&crypto_q_mtx)
#define	CRYPTO_Q_UNLOCK()	mtx_unlock(&crypto_q_mtx)

/*
 * There are two queues for processing completed crypto requests; one
 * for the symmetric and one for the asymmetric ops.  We only need one
 * but have two to avoid type futzing (cryptop vs. cryptkop).  A single
 * mutex is used to lock access to both queues.  Note that this lock
 * must be separate from the lock on request queues to insure driver
 * callbacks don't generate lock order reversals.
 */
static	TAILQ_HEAD(,cryptop) crp_ret_q;		/* callback queues */
static	TAILQ_HEAD(,cryptkop) crp_ret_kq;
static	struct mtx crypto_ret_q_mtx;
#define	CRYPTO_RETQ_LOCK()	mtx_lock(&crypto_ret_q_mtx)
#define	CRYPTO_RETQ_UNLOCK()	mtx_unlock(&crypto_ret_q_mtx)

static	uma_zone_t cryptop_zone;
static	uma_zone_t cryptodesc_zone;

int	crypto_usercrypto = 1;		/* userland may open /dev/crypto */
SYSCTL_INT(_kern, OID_AUTO, usercrypto, CTLFLAG_RW,
	   &crypto_usercrypto, 0,
	   "Enable/disable user-mode access to crypto support");
int	crypto_userasymcrypto = 1;	/* userland may do asym crypto reqs */
SYSCTL_INT(_kern, OID_AUTO, userasymcrypto, CTLFLAG_RW,
	   &crypto_userasymcrypto, 0,
	   "Enable/disable user-mode access to asymmetric crypto support");
int	crypto_devallowsoft = 0;	/* only use hardware crypto for asym */
SYSCTL_INT(_kern, OID_AUTO, cryptodevallowsoft, CTLFLAG_RW,
	   &crypto_devallowsoft, 0,
	   "Enable/disable use of software asym crypto support");

MALLOC_DEFINE(M_CRYPTO_DATA, "crypto", "crypto session records");

static void
crypto_init(void)
{
	cryptop_zone = uma_zcreate("cryptop", sizeof (struct cryptop),
				    0, 0, 0, 0,
				    UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	cryptodesc_zone = uma_zcreate("cryptodesc", sizeof (struct cryptodesc),
				    0, 0, 0, 0,
				    UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	if (cryptodesc_zone == NULL || cryptop_zone == NULL)
		panic("cannot setup crypto zones");

	mtx_init(&crypto_drivers_mtx, "crypto driver table",
		NULL, MTX_DEF|MTX_QUIET);

	crypto_drivers_num = CRYPTO_DRIVERS_INITIAL;
	crypto_drivers = malloc(crypto_drivers_num *
	    sizeof(struct cryptocap), M_CRYPTO_DATA, M_NOWAIT | M_ZERO);
	if (crypto_drivers == NULL)
		panic("cannot setup crypto drivers");

	TAILQ_INIT(&crp_q);
	TAILQ_INIT(&crp_kq);
	mtx_init(&crypto_q_mtx, "crypto op queues", NULL, MTX_DEF);

	TAILQ_INIT(&crp_ret_q);
	TAILQ_INIT(&crp_ret_kq);
	mtx_init(&crypto_ret_q_mtx, "crypto return queues", NULL, MTX_DEF);
}
SYSINIT(crypto_init, SI_SUB_DRIVERS, SI_ORDER_FIRST, crypto_init, NULL)

/*
 * Create a new session.
 */
int
crypto_newsession(u_int64_t *sid, struct cryptoini *cri, int hard)
{
	struct cryptoini *cr;
	u_int32_t hid, lid;
	int err = EINVAL;

	CRYPTO_DRIVER_LOCK();

	if (crypto_drivers == NULL)
		goto done;

	/*
	 * The algorithm we use here is pretty stupid; just use the
	 * first driver that supports all the algorithms we need.
	 *
	 * XXX We need more smarts here (in real life too, but that's
	 * XXX another story altogether).
	 */

	for (hid = 0; hid < crypto_drivers_num; hid++) {
		/*
		 * If it's not initialized or has remaining sessions
		 * referencing it, skip.
		 */
		if (crypto_drivers[hid].cc_newsession == NULL ||
		    (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP))
			continue;

		/* Hardware required -- ignore software drivers. */
		if (hard > 0 &&
		    (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_SOFTWARE))
			continue;
		/* Software required -- ignore hardware drivers. */
		if (hard < 0 &&
		    (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_SOFTWARE) == 0)
			continue;

		/* See if all the algorithms are supported. */
		for (cr = cri; cr; cr = cr->cri_next)
			if (crypto_drivers[hid].cc_alg[cr->cri_alg] == 0)
				break;

		if (cr == NULL) {
			/* Ok, all algorithms are supported. */

			/*
			 * Can't do everything in one session.
			 *
			 * XXX Fix this. We need to inject a "virtual" session layer right
			 * XXX about here.
			 */

			/* Call the driver initialization routine. */
			lid = hid;		/* Pass the driver ID. */
			err = crypto_drivers[hid].cc_newsession(
					crypto_drivers[hid].cc_arg, &lid, cri);
			if (err == 0) {
				(*sid) = hid;
				(*sid) <<= 32;
				(*sid) |= (lid & 0xffffffff);
				crypto_drivers[hid].cc_sessions++;
			}
			break;
		}
	}
done:
	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Delete an existing session (or a reserved session on an unregistered
 * driver).
 */
int
crypto_freesession(u_int64_t sid)
{
	u_int32_t hid;
	int err;

	CRYPTO_DRIVER_LOCK();

	if (crypto_drivers == NULL) {
		err = EINVAL;
		goto done;
	}

	/* Determine two IDs. */
	hid = SESID2HID(sid);

	if (hid >= crypto_drivers_num) {
		err = ENOENT;
		goto done;
	}

	if (crypto_drivers[hid].cc_sessions)
		crypto_drivers[hid].cc_sessions--;

	/* Call the driver cleanup routine, if available. */
	if (crypto_drivers[hid].cc_freesession)
		err = crypto_drivers[hid].cc_freesession(
				crypto_drivers[hid].cc_arg, sid);
	else
		err = 0;

	/*
	 * If this was the last session of a driver marked as invalid,
	 * make the entry available for reuse.
	 */
	if ((crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP) &&
	    crypto_drivers[hid].cc_sessions == 0)
		bzero(&crypto_drivers[hid], sizeof(struct cryptocap));

done:
	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Return an unused driver id.  Used by drivers prior to registering
 * support for the algorithms they handle.
 */
int32_t
crypto_get_driverid(u_int32_t flags)
{
	struct cryptocap *newdrv;
	int i;

	CRYPTO_DRIVER_LOCK();

	for (i = 0; i < crypto_drivers_num; i++)
		if (crypto_drivers[i].cc_process == NULL &&
		    (crypto_drivers[i].cc_flags & CRYPTOCAP_F_CLEANUP) == 0 &&
		    crypto_drivers[i].cc_sessions == 0)
			break;

	/* Out of entries, allocate some more. */
	if (i == crypto_drivers_num) {
		/* Be careful about wrap-around. */
		if (2 * crypto_drivers_num <= crypto_drivers_num) {
			CRYPTO_DRIVER_UNLOCK();
			printf("crypto: driver count wraparound!\n");
			return -1;
		}

		newdrv = malloc(2 * crypto_drivers_num *
		    sizeof(struct cryptocap), M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
		if (newdrv == NULL) {
			CRYPTO_DRIVER_UNLOCK();
			printf("crypto: no space to expand driver table!\n");
			return -1;
		}

		bcopy(crypto_drivers, newdrv,
		    crypto_drivers_num * sizeof(struct cryptocap));

		crypto_drivers_num *= 2;

		free(crypto_drivers, M_CRYPTO_DATA);
		crypto_drivers = newdrv;
	}

	/* NB: state is zero'd on free */
	crypto_drivers[i].cc_sessions = 1;	/* Mark */
	crypto_drivers[i].cc_flags = flags;
	if (bootverbose)
		printf("crypto: assign driver %u, flags %u\n", i, flags);

	CRYPTO_DRIVER_UNLOCK();

	return i;
}

static struct cryptocap *
crypto_checkdriver(u_int32_t hid)
{
	if (crypto_drivers == NULL)
		return NULL;
	return (hid >= crypto_drivers_num ? NULL : &crypto_drivers[hid]);
}

/*
 * Register support for a key-related algorithm.  This routine
 * is called once for each algorithm supported a driver.
 */
int
crypto_kregister(u_int32_t driverid, int kalg, u_int32_t flags,
    int (*kprocess)(void*, struct cryptkop *, int),
    void *karg)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_DRIVER_LOCK();

	cap = crypto_checkdriver(driverid);
	if (cap != NULL &&
	    (CRK_ALGORITM_MIN <= kalg && kalg <= CRK_ALGORITHM_MAX)) {
		/*
		 * XXX Do some performance testing to determine placing.
		 * XXX We probably need an auxiliary data structure that
		 * XXX describes relative performances.
		 */

		cap->cc_kalg[kalg] = flags | CRYPTO_ALG_FLAG_SUPPORTED;
		if (bootverbose)
			printf("crypto: driver %u registers key alg %u flags %u\n"
				, driverid
				, kalg
				, flags
			);

		if (cap->cc_kprocess == NULL) {
			cap->cc_karg = karg;
			cap->cc_kprocess = kprocess;
		}
		err = 0;
	} else
		err = EINVAL;

	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Register support for a non-key-related algorithm.  This routine
 * is called once for each such algorithm supported by a driver.
 */
int
crypto_register(u_int32_t driverid, int alg, u_int16_t maxoplen,
    u_int32_t flags,
    int (*newses)(void*, u_int32_t*, struct cryptoini*),
    int (*freeses)(void*, u_int64_t),
    int (*process)(void*, struct cryptop *, int),
    void *arg)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_DRIVER_LOCK();

	cap = crypto_checkdriver(driverid);
	/* NB: algorithms are in the range [1..max] */
	if (cap != NULL &&
	    (CRYPTO_ALGORITHM_MIN <= alg && alg <= CRYPTO_ALGORITHM_MAX)) {
		/*
		 * XXX Do some performance testing to determine placing.
		 * XXX We probably need an auxiliary data structure that
		 * XXX describes relative performances.
		 */

		cap->cc_alg[alg] = flags | CRYPTO_ALG_FLAG_SUPPORTED;
		cap->cc_max_op_len[alg] = maxoplen;
		if (bootverbose)
			printf("crypto: driver %u registers alg %u flags %u maxoplen %u\n"
				, driverid
				, alg
				, flags
				, maxoplen
			);

		if (cap->cc_process == NULL) {
			cap->cc_arg = arg;
			cap->cc_newsession = newses;
			cap->cc_process = process;
			cap->cc_freesession = freeses;
			cap->cc_sessions = 0;		/* Unmark */
		}
		err = 0;
	} else
		err = EINVAL;

	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Unregister a crypto driver. If there are pending sessions using it,
 * leave enough information around so that subsequent calls using those
 * sessions will correctly detect the driver has been unregistered and
 * reroute requests.
 */
int
crypto_unregister(u_int32_t driverid, int alg)
{
	int i, err;
	u_int32_t ses;
	struct cryptocap *cap;

	CRYPTO_DRIVER_LOCK();

	cap = crypto_checkdriver(driverid);
	if (cap != NULL &&
	    (CRYPTO_ALGORITHM_MIN <= alg && alg <= CRYPTO_ALGORITHM_MAX) &&
	    cap->cc_alg[alg] != 0) {
		cap->cc_alg[alg] = 0;
		cap->cc_max_op_len[alg] = 0;

		/* Was this the last algorithm ? */
		for (i = 1; i <= CRYPTO_ALGORITHM_MAX; i++)
			if (cap->cc_alg[i] != 0)
				break;

		if (i == CRYPTO_ALGORITHM_MAX + 1) {
			ses = cap->cc_sessions;
			bzero(cap, sizeof(struct cryptocap));
			if (ses != 0) {
				/*
				 * If there are pending sessions, just mark as invalid.
				 */
				cap->cc_flags |= CRYPTOCAP_F_CLEANUP;
				cap->cc_sessions = ses;
			}
		}
		err = 0;
	} else
		err = EINVAL;

	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Unregister all algorithms associated with a crypto driver.
 * If there are pending sessions using it, leave enough information
 * around so that subsequent calls using those sessions will
 * correctly detect the driver has been unregistered and reroute
 * requests.
 */
int
crypto_unregister_all(u_int32_t driverid)
{
	int i, err;
	u_int32_t ses;
	struct cryptocap *cap;

	CRYPTO_DRIVER_LOCK();

	cap = crypto_checkdriver(driverid);
	if (cap != NULL) {
		for (i = CRYPTO_ALGORITHM_MIN; i <= CRYPTO_ALGORITHM_MAX; i++) {
			cap->cc_alg[i] = 0;
			cap->cc_max_op_len[i] = 0;
		}
		ses = cap->cc_sessions;
		bzero(cap, sizeof(struct cryptocap));
		if (ses != 0) {
			/*
			 * If there are pending sessions, just mark as invalid.
			 */
			cap->cc_flags |= CRYPTOCAP_F_CLEANUP;
			cap->cc_sessions = ses;
		}
		err = 0;
	} else
		err = EINVAL;

	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Clear blockage on a driver.  The what parameter indicates whether
 * the driver is now ready for cryptop's and/or cryptokop's.
 */
int
crypto_unblock(u_int32_t driverid, int what)
{
	struct cryptocap *cap;
	int needwakeup, err;

	needwakeup = 0;

	CRYPTO_Q_LOCK();
	cap = crypto_checkdriver(driverid);
	if (cap != NULL) {
		if (what & CRYPTO_SYMQ) {
			needwakeup |= cap->cc_qblocked;
			cap->cc_qblocked = 0;
		}
		if (what & CRYPTO_ASYMQ) {
			needwakeup |= cap->cc_kqblocked;
			cap->cc_kqblocked = 0;
		}
		err = 0;
	} else
		err = EINVAL;
	CRYPTO_Q_UNLOCK();

	if (needwakeup)
		wakeup_one(&crp_q);

	return err;
}

/*
 * Add a crypto request to a queue, to be processed by the kernel thread.
 */
int
crypto_dispatch(struct cryptop *crp)
{
	struct cryptocap *cap;
	int wasempty;

	CRYPTO_Q_LOCK();
	wasempty = TAILQ_EMPTY(&crp_q);
	TAILQ_INSERT_TAIL(&crp_q, crp, crp_next);

	/*
	 * Wakeup processing thread if driver is not blocked.
	 */
	cap = crypto_checkdriver(SESID2HID(crp->crp_sid));
	if (cap && !cap->cc_qblocked && wasempty)
		wakeup_one(&crp_q);
	CRYPTO_Q_UNLOCK();

	return 0;
}

/*
 * Add an asymetric crypto request to a queue,
 * to be processed by the kernel thread.
 */
int
crypto_kdispatch(struct cryptkop *krp)
{
	struct cryptocap *cap;
	int wasempty;

	CRYPTO_Q_LOCK();
	wasempty = TAILQ_EMPTY(&crp_kq);
	TAILQ_INSERT_TAIL(&crp_kq, krp, krp_next);

	/*
	 * Wakeup processing thread if driver is not blocked.
	 */
	cap = crypto_checkdriver(krp->krp_hid);
	if (cap && !cap->cc_kqblocked && wasempty)
		wakeup_one(&crp_q);	/* NB: shared wait channel */
	CRYPTO_Q_UNLOCK();

	return 0;
}

/*
 * Dispatch an assymetric crypto request to the appropriate crypto devices.
 */
static int
crypto_kinvoke(struct cryptkop *krp, int hint)
{
	u_int32_t hid;
	int error;

	mtx_assert(&crypto_q_mtx, MA_OWNED);

	/* Sanity checks. */
	if (krp == NULL || krp->krp_callback == NULL)
		return EINVAL;

	for (hid = 0; hid < crypto_drivers_num; hid++) {
		if ((crypto_drivers[hid].cc_flags & CRYPTOCAP_F_SOFTWARE) &&
		    !crypto_devallowsoft)
			continue;
		if (crypto_drivers[hid].cc_kprocess == NULL)
			continue;
		if ((crypto_drivers[hid].cc_kalg[krp->krp_op] &
		    CRYPTO_ALG_FLAG_SUPPORTED) == 0)
			continue;
		break;
	}
	if (hid < crypto_drivers_num) {
		krp->krp_hid = hid;
		error = crypto_drivers[hid].cc_kprocess(
				crypto_drivers[hid].cc_karg, krp, hint);
	} else
		error = ENODEV;

	if (error) {
		krp->krp_status = error;
		CRYPTO_RETQ_LOCK();
		TAILQ_INSERT_TAIL(&crp_ret_kq, krp, krp_next);
		CRYPTO_RETQ_UNLOCK();
	}
	return 0;
}

/*
 * Dispatch a crypto request to the appropriate crypto devices.
 */
static int
crypto_invoke(struct cryptop *crp, int hint)
{
	u_int32_t hid;
	int (*process)(void*, struct cryptop *, int);

	mtx_assert(&crypto_q_mtx, MA_OWNED);

	/* Sanity checks. */
	if (crp == NULL || crp->crp_callback == NULL)
		return EINVAL;

	if (crp->crp_desc == NULL) {
		crp->crp_etype = EINVAL;
		CRYPTO_RETQ_LOCK();
		TAILQ_INSERT_TAIL(&crp_ret_q, crp, crp_next);
		CRYPTO_RETQ_UNLOCK();
		return 0;
	}

	hid = SESID2HID(crp->crp_sid);
	if (hid < crypto_drivers_num) {
		if (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP)
			crypto_freesession(crp->crp_sid);
		process = crypto_drivers[hid].cc_process;
	} else {
		process = NULL;
	}

	if (process == NULL) {
		struct cryptodesc *crd;
		u_int64_t nid;

		/*
		 * Driver has unregistered; migrate the session and return
		 * an error to the caller so they'll resubmit the op.
		 */
		for (crd = crp->crp_desc; crd->crd_next; crd = crd->crd_next)
			crd->CRD_INI.cri_next = &(crd->crd_next->CRD_INI);

		if (crypto_newsession(&nid, &(crp->crp_desc->CRD_INI), 0) == 0)
			crp->crp_sid = nid;

		crp->crp_etype = EAGAIN;
		CRYPTO_RETQ_LOCK();
		TAILQ_INSERT_TAIL(&crp_ret_q, crp, crp_next);
		CRYPTO_RETQ_UNLOCK();
		return 0;
	} else {
		/*
		 * Invoke the driver to process the request.
		 */
		return (*process)(crypto_drivers[hid].cc_arg, crp, hint);
	}
}

/*
 * Release a set of crypto descriptors.
 */
void
crypto_freereq(struct cryptop *crp)
{
	struct cryptodesc *crd;

	if (crp == NULL)
		return;

	while ((crd = crp->crp_desc) != NULL) {
		crp->crp_desc = crd->crd_next;
		uma_zfree(cryptodesc_zone, crd);
	}

	uma_zfree(cryptop_zone, crp);
}

/*
 * Acquire a set of crypto descriptors.
 */
struct cryptop *
crypto_getreq(int num)
{
	struct cryptodesc *crd;
	struct cryptop *crp;

	crp = uma_zalloc(cryptop_zone, 0);
	if (crp != NULL) {
		while (num--) {
			crd = uma_zalloc(cryptodesc_zone, 0);
			if (crd == NULL) {
				crypto_freereq(crp);
				return NULL;
			}

			crd->crd_next = crp->crp_desc;
			crp->crp_desc = crd;
		}
	}
	return crp;
}

/*
 * Invoke the callback on behalf of the driver.
 */
void
crypto_done(struct cryptop *crp)
{
	int wasempty;

	CRYPTO_RETQ_LOCK();
	wasempty = TAILQ_EMPTY(&crp_ret_q);
	TAILQ_INSERT_TAIL(&crp_ret_q, crp, crp_next);
	CRYPTO_RETQ_UNLOCK();

	if (wasempty)
		wakeup_one(&crp_q);	/* shared wait channel */
}

/*
 * Invoke the callback on behalf of the driver.
 */
void
crypto_kdone(struct cryptkop *krp)
{
	int wasempty;

	CRYPTO_RETQ_LOCK();
	wasempty = TAILQ_EMPTY(&crp_ret_kq);
	TAILQ_INSERT_TAIL(&crp_ret_kq, krp, krp_next);
	CRYPTO_RETQ_UNLOCK();

	if (wasempty)
		wakeup_one(&crp_q);	/* shared wait channel */
}

int
crypto_getfeat(int *featp)
{
	int hid, kalg, feat = 0;

	if (!crypto_userasymcrypto)
		goto out;	  

	CRYPTO_DRIVER_LOCK();
	for (hid = 0; hid < crypto_drivers_num; hid++) {
		if ((crypto_drivers[hid].cc_flags & CRYPTOCAP_F_SOFTWARE) &&
		    !crypto_devallowsoft) {
			continue;
		}
		if (crypto_drivers[hid].cc_kprocess == NULL)
			continue;
		for (kalg = 0; kalg < CRK_ALGORITHM_MAX; kalg++)
			if ((crypto_drivers[hid].cc_kalg[kalg] &
			    CRYPTO_ALG_FLAG_SUPPORTED) != 0)
				feat |=  1 << kalg;
	}
	CRYPTO_DRIVER_UNLOCK();
out:
	*featp = feat;
	return (0);
}

static struct proc *cryptoproc;

static void
crypto_shutdown(void *arg, int howto)
{
	/* XXX flush queues */
}

/*
 * Crypto thread, runs as a kernel thread to process crypto requests.
 */
static void
crypto_proc(void)
{
	struct cryptop *crp, *crpt, *submit;
	struct cryptkop *krp, *krpt;
	struct cryptocap *cap;
	int result, hint;

	mtx_lock(&Giant);		/* XXX for msleep */

	EVENTHANDLER_REGISTER(shutdown_pre_sync, crypto_shutdown, NULL,
			      SHUTDOWN_PRI_FIRST);

	for (;;) {
		/*
		 * Find the first element in the queue that can be
		 * processed and look-ahead to see if multiple ops
		 * are ready for the same driver.
		 */
		submit = NULL;
		hint = 0;
		CRYPTO_Q_LOCK();
		TAILQ_FOREACH(crp, &crp_q, crp_next) {
			u_int32_t hid = SESID2HID(crp->crp_sid);
			cap = crypto_checkdriver(hid);
			if (cap == NULL || cap->cc_process == NULL) {
				/* Op needs to be migrated, process it. */
				if (submit == NULL)
					submit = crp;
				break;
			}
			if (!cap->cc_qblocked) {
				if (submit != NULL) {
					/*
					 * We stop on finding another op,
					 * regardless whether its for the same
					 * driver or not.  We could keep
					 * searching the queue but it might be
					 * better to just use a per-driver
					 * queue instead.
					 */
					if (SESID2HID(submit->crp_sid) == hid)
						hint = CRYPTO_HINT_MORE;
					break;
				} else {
					submit = crp;
					if (submit->crp_flags & CRYPTO_F_NODELAY)
						break;
					/* keep scanning for more are q'd */
				}
			}
		}
		if (submit != NULL) {
			TAILQ_REMOVE(&crp_q, submit, crp_next);
			result = crypto_invoke(submit, hint);
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptop's and put
				 * the request back in the queue.  It would
				 * best to put the request back where we got
				 * it but that's hard so for now we put it
				 * at the front.  This should be ok; putting
				 * it at the end does not work.
				 */
				/* XXX validate sid again? */
				crypto_drivers[SESID2HID(submit->crp_sid)].cc_qblocked = 1;
				TAILQ_INSERT_HEAD(&crp_q, submit, crp_next);
			}
		}

		/* As above, but for key ops */
		TAILQ_FOREACH(krp, &crp_kq, krp_next) {
			cap = crypto_checkdriver(krp->krp_hid);
			if (cap == NULL || cap->cc_kprocess == NULL) {
				/* Op needs to be migrated, process it. */
				break;
			}
			if (!cap->cc_kqblocked)
				break;
		}
		if (krp != NULL) {
			TAILQ_REMOVE(&crp_kq, krp, krp_next);
			result = crypto_kinvoke(krp, 0);
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptkop's and put
				 * the request back in the queue.  It would
				 * best to put the request back where we got
				 * it but that's hard so for now we put it
				 * at the front.  This should be ok; putting
				 * it at the end does not work.
				 */
				/* XXX validate sid again? */
				crypto_drivers[krp->krp_hid].cc_kqblocked = 1;
				TAILQ_INSERT_HEAD(&crp_kq, krp, krp_next);
			}
		}
		CRYPTO_Q_UNLOCK();

		/* Harvest return q for completed ops */
		CRYPTO_RETQ_LOCK();
		crpt = TAILQ_FIRST(&crp_ret_q);
		if (crpt != NULL)
			TAILQ_REMOVE(&crp_ret_q, crpt, crp_next);
		CRYPTO_RETQ_UNLOCK();

		if (crpt != NULL)
			crpt->crp_callback(crpt);

		/* Harvest return q for completed kops */
		CRYPTO_RETQ_LOCK();
		krpt = TAILQ_FIRST(&crp_ret_kq);
		if (krpt != NULL)
			TAILQ_REMOVE(&crp_ret_kq, krpt, krp_next);
		CRYPTO_RETQ_UNLOCK();

		if (krpt != NULL)
			krp->krp_callback(krp);

		if (crp == NULL && krp == NULL && crpt == NULL && krpt == NULL) {
			/*
			 * Nothing more to be processed.  Sleep until we're
			 * woken because there are more ops to process.
			 * This happens either by submission or by a driver
			 * becoming unblocked and notifying us through
			 * crypto_unblock.  Note that when we wakeup we
			 * start processing each queue again from the
			 * front. It's not clear that it's important to
			 * preserve this ordering since ops may finish
			 * out of order if dispatched to different devices
			 * and some become blocked while others do not.
			 */
			tsleep(&crp_q, PWAIT, "crypto_wait", 0);
		}
	}
}

static struct kproc_desc crypto_kp = {
	"crypto",
	crypto_proc,
	&cryptoproc
};
SYSINIT(crypto_proc, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, kproc_start, &crypto_kp)
