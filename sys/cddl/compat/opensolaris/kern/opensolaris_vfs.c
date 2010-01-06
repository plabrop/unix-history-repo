/*-
 * Copyright (c) 2006-2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/priv.h>
#include <sys/libkern.h>

MALLOC_DECLARE(M_MOUNT);

void
vfs_setmntopt(vfs_t *vfsp, const char *name, const char *arg,
    int flags __unused)
{
	struct vfsopt *opt;
	size_t namesize;
	int locked;

	if (!(locked = mtx_owned(MNT_MTX(vfsp))))
		MNT_ILOCK(vfsp);

	if (vfsp->mnt_opt == NULL) {
		void *opts;

		MNT_IUNLOCK(vfsp);
		opts = malloc(sizeof(*vfsp->mnt_opt), M_MOUNT, M_WAITOK);
		MNT_ILOCK(vfsp);
		if (vfsp->mnt_opt == NULL) {
			vfsp->mnt_opt = opts;
			TAILQ_INIT(vfsp->mnt_opt);
		} else {
			free(opts, M_MOUNT);
		}
	}

	MNT_IUNLOCK(vfsp);

	opt = malloc(sizeof(*opt), M_MOUNT, M_WAITOK);
	namesize = strlen(name) + 1;
	opt->name = malloc(namesize, M_MOUNT, M_WAITOK);
	strlcpy(opt->name, name, namesize);
	opt->pos = -1;
	opt->seen = 1;
	if (arg == NULL) {
		opt->value = NULL;
		opt->len = 0;
	} else {
		opt->len = strlen(arg) + 1;
		opt->value = malloc(opt->len, M_MOUNT, M_WAITOK);
		bcopy(arg, opt->value, opt->len);
	}

	MNT_ILOCK(vfsp);
	TAILQ_INSERT_TAIL(vfsp->mnt_opt, opt, link);
	if (!locked)
		MNT_IUNLOCK(vfsp);
}

void
vfs_clearmntopt(vfs_t *vfsp, const char *name)
{
	int locked;

	if (!(locked = mtx_owned(MNT_MTX(vfsp))))
		MNT_ILOCK(vfsp);
	vfs_deleteopt(vfsp->mnt_opt, name);
	if (!locked)
		MNT_IUNLOCK(vfsp);
}

int
vfs_optionisset(const vfs_t *vfsp, const char *opt, char **argp)
{
	struct vfsoptlist *opts = vfsp->mnt_optnew;
	int error;

	if (opts == NULL)
		return (0);
	error = vfs_getopt(opts, opt, (void **)argp, NULL);
	return (error != 0 ? 0 : 1);
}

extern struct mount *vfs_mount_alloc(struct vnode *vp, struct vfsconf *vfsp,
    const char *fspath, struct thread *td);

int
mount_snapshot(kthread_t *td, vnode_t **vpp, const char *fstype, char *fspath,
    char *fspec, int fsflags)
{
	struct mount *mp;
	struct vfsconf *vfsp;
	struct ucred *cr;
	vnode_t *vp;
	int error;

	/*
	 * Be ultra-paranoid about making sure the type and fspath
	 * variables will fit in our mp buffers, including the
	 * terminating NUL.
	 */
	if (strlen(fstype) >= MFSNAMELEN || strlen(fspath) >= MNAMELEN)
		return (ENAMETOOLONG);

	vfsp = vfs_byname_kld(fstype, td, &error);
	if (vfsp == NULL)
		return (ENODEV);

	vp = *vpp;
	if (vp->v_type != VDIR)
		return (ENOTDIR);
	/*
	 * We need vnode lock to protect v_mountedhere and vnode interlock
	 * to protect v_iflag.
	 */
	vn_lock(vp, LK_SHARED | LK_RETRY, td);
	VI_LOCK(vp);
	if ((vp->v_iflag & VI_MOUNT) != 0 || vp->v_mountedhere != NULL) {
		VI_UNLOCK(vp);
		VOP_UNLOCK(vp, 0, td);
		return (EBUSY);
	}
	vp->v_iflag |= VI_MOUNT;
	VI_UNLOCK(vp);
	VOP_UNLOCK(vp, 0, td);

	/*
	 * Allocate and initialize the filesystem.
	 */
	mp = vfs_mount_alloc(vp, vfsp, fspath, td);

	mp->mnt_optnew = NULL;
	vfs_setmntopt(mp, "from", fspec, 0);
	mp->mnt_optnew = mp->mnt_opt;
	mp->mnt_opt = NULL;

	/*
	 * Set the mount level flags.
	 */
	mp->mnt_flag &= ~MNT_UPDATEMASK;
	mp->mnt_flag |= fsflags & (MNT_UPDATEMASK | MNT_FORCE | MNT_ROOTFS);
	/*
	 * Snapshots are always read-only.
	 */
	mp->mnt_flag |= MNT_RDONLY;
#if 0
	/*
	 * We don't want snapshots to be visible in regular
	 * mount(8) and df(1) output.
	 */
	mp->mnt_flag |= MNT_IGNORE;
#endif
	/*
	 * Unprivileged user can trigger mounting a snapshot, but we don't want
	 * him to unmount it, so we switch to privileged of original mount.
	 */
	crfree(mp->mnt_cred);
	mp->mnt_cred = crdup(vp->v_mount->mnt_cred);
	mp->mnt_stat.f_owner = mp->mnt_cred->cr_uid;
	/*
	 * XXX: This is evil, but we can't mount a snapshot as a regular user.
	 * XXX: Is is safe when snapshot is mounted from within a jail?
	 */
	cr = td->td_ucred;
	td->td_ucred = kcred;
	error = VFS_MOUNT(mp, td);
	td->td_ucred = cr;

	if (error == 0) {
		if (mp->mnt_opt != NULL)
			vfs_freeopts(mp->mnt_opt);
		mp->mnt_opt = mp->mnt_optnew;
		(void)VFS_STATFS(mp, &mp->mnt_stat, td);
	}
	/*
	 * Prevent external consumers of mount options from reading
	 * mnt_optnew.
	*/
	mp->mnt_optnew = NULL;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef FREEBSD_NAMECACHE
	cache_purge(vp);
#endif
	VI_LOCK(vp);
	vp->v_iflag &= ~VI_MOUNT;
	VI_UNLOCK(vp);
	if (error == 0) {
		vnode_t *mvp;

		vp->v_mountedhere = mp;
		/*
		 * Put the new filesystem on the mount list.
		 */
		mtx_lock(&mountlist_mtx);
		TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
		mtx_unlock(&mountlist_mtx);
		vfs_event_signal(NULL, VQ_MOUNT, 0);
		if (VFS_ROOT(mp, LK_EXCLUSIVE, &mvp, td))
			panic("mount: lost mount");
		vput(vp);
		vfs_unbusy(mp, td);
		*vpp = mvp;
	} else {
		vput(vp);
		vfs_unbusy(mp, td);
		vfs_mount_destroy(mp);
		*vpp = NULL;
	}
	return (error);
}
