/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)ufs_lookup.c	7.37 (Berkeley) %G%
 */

#include <sys/param.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

struct	nchstats nchstats;
#ifdef DIAGNOSTIC
int	dirchk = 1;
#else
int	dirchk = 0;
#endif

/*
 * Convert a component of a pathname into a pointer to a locked inode.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The cnp->cn_nameiop argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and iput
 * instead of two iputs.
 *
 * Overall outline of ufs_lookup:
 *
 *	check accessibility of directory
 *	look for name in cache, if found, then if at end of path
 *	  and deleting or creating, drop it, else return name
 *	search for name in directory, to found or notfound
 * notfound:
 *	if creating, return locked directory, leaving info on available slots
 *	else return error
 * found:
 *	if at end of path and deleting, return information to allow delete
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 *
 * NOTE: (LOOKUP | LOCKPARENT) currently returns the parent inode unlocked.
 */
int
ufs_lookup(dvp, vpp, cnp)   /* converted to CN */
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	register struct inode *dp;	/* the directory we are searching */
	struct buf *bp;			/* a buffer of directory entries */
	register struct direct *ep;	/* the current directory entry */
	int entryoffsetinblock;		/* offset of ep in bp's buffer */
	enum {NONE, COMPACT, FOUND} slotstatus;
	int slotoffset;			/* offset of area with free space */
	int slotsize;			/* size of area at slotoffset */
	int slotfreespace;		/* amount of space free in slot */
	int slotneeded;			/* size of the entry we're seeking */
	int numdirpasses;		/* strategy for directory search */
	int endsearch;			/* offset to end directory search */
	int prevoff;			/* prev entry ndp->ni_ufs.ufs_offset */
	struct inode *pdp;		/* saved dp during symlink work */
	struct vnode *tdp;		/* returned by VOP_VGET */
	off_t enduseful;		/* pointer past last used dir slot */
	u_long bmask;			/* block offset mask */
	int lockparent;			/* 1 => lockparent flag is set */
	int wantparent;			/* 1 => wantparent or lockparent flag */
	int error;
	struct vnode *vdp = dvp;	/* saved for one special case */

	bp = NULL;
	slotoffset = -1;
	*vpp = NULL;
	dp = VTOI(dvp);
	lockparent = cnp->cn_flags & LOCKPARENT;
	wantparent = cnp->cn_flags & (LOCKPARENT|WANTPARENT);

	/*
	 * Check accessiblity of directory.
	 */
	if ((dp->i_mode&IFMT) != IFDIR)
		return (ENOTDIR);
	if (error = ufs_access(dvp, VEXEC, cnp->cn_cred, cnp->cn_proc))
		return (error);

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if (error = cache_lookup(dvp, vpp, cnp)) {
		int vpid;	/* capability number of vnode */

		if (error == ENOENT)
			return (error);
#ifdef PARANOID
		if (dvp == ndp->ni_rdir && (cnp->cn_flags&ISDOTDOT))
			panic("ufs_lookup: .. through root");
#endif
		/*
		 * Get the next vnode in the path.
		 * See comment below starting `Step through' for
		 * an explaination of the locking protocol.
		 */
		/*
		 * NEEDSWORK: The borrowing of variables
		 * here is quite confusing.  Usually, dvp/dp
		 * is the directory being searched.
		 * Here it's the target returned from the cache.
		 */
		pdp = dp;
		dp = VTOI(*vpp);
		dvp = *vpp;
		vpid = dvp->v_id;
		if (pdp == dp) {   /* lookup on "." */
			VREF(dvp);
			error = 0;
		} else if (cnp->cn_flags&ISDOTDOT) {
			IUNLOCK(pdp);
			error = vget(dvp);
			if (!error && lockparent && (cnp->cn_flags&ISLASTCN))
				ILOCK(pdp);
		} else {
			error = vget(dvp);
			if (!lockparent || error || !(cnp->cn_flags&ISLASTCN))
				IUNLOCK(pdp);
		}
		/*
		 * Check that the capability number did not change
		 * while we were waiting for the lock.
		 */
		if (!error) {
			if (vpid == dvp->v_id)
				return (0);
			ufs_iput(dp);
			if (lockparent && pdp != dp && (cnp->cn_flags&ISLASTCN))
				IUNLOCK(pdp);
		}
		ILOCK(pdp);
		dp = pdp;
		dvp = ITOV(dp);
		*vpp = NULL;
	}

	/*
	 * Suppress search for slots unless creating
	 * file and at end of pathname, in which case
	 * we watch for a place to put the new file in
	 * case it doesn't already exist.
	 */
	slotstatus = FOUND;
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) && (cnp->cn_flags&ISLASTCN)) {
		slotstatus = NONE;
		slotfreespace = 0;
		slotneeded = ((sizeof (struct direct) - (MAXNAMLEN + 1)) +
			((cnp->cn_namelen + 1 + 3) &~ 3));
	}

	/*
	 * If there is cached information on a previous search of
	 * this directory, pick up where we last left off.
	 * We cache only lookups as these are the most common
	 * and have the greatest payoff. Caching CREATE has little
	 * benefit as it usually must search the entire directory
	 * to determine that the entry does not exist. Caching the
	 * location of the last DELETE or RENAME has not reduced
	 * profiling time and hence has been removed in the interest
	 * of simplicity.
	 */
	bmask = VFSTOUFS(dvp->v_mount)->um_mountp->mnt_stat.f_iosize - 1;
	if (cnp->cn_nameiop != LOOKUP || dp->i_diroff == 0 || dp->i_diroff > dp->i_size) {
		cnp->cn_ufs.ufs_offset = 0;
		numdirpasses = 1;
	} else {
		cnp->cn_ufs.ufs_offset = dp->i_diroff;
		if ((entryoffsetinblock = cnp->cn_ufs.ufs_offset & bmask) &&
		    (error = VOP_BLKATOFF(dvp, cnp->cn_ufs.ufs_offset, NULL,
		     &bp)))
			return (error);
		numdirpasses = 2;
		nchstats.ncs_2passes++;
	}
	endsearch = roundup(dp->i_size, DIRBLKSIZ);
	enduseful = 0;

searchloop:
	while (cnp->cn_ufs.ufs_offset < endsearch) {
		/*
		 * If offset is on a block boundary, read the next directory
		 * block.  Release previous if it exists.
		 */
		if ((cnp->cn_ufs.ufs_offset & bmask) == 0) {
			if (bp != NULL)
				brelse(bp);
			if (error = VOP_BLKATOFF(dvp, cnp->cn_ufs.ufs_offset,
			    NULL, &bp))
				return (error);
			entryoffsetinblock = 0;
		}
		/*
		 * If still looking for a slot, and at a DIRBLKSIZE
		 * boundary, have to start looking for free space again.
		 */
		if (slotstatus == NONE &&
		    (entryoffsetinblock & (DIRBLKSIZ - 1)) == 0) {
			slotoffset = -1;
			slotfreespace = 0;
		}
		/*
		 * Get pointer to next entry.
		 * Full validation checks are slow, so we only check
		 * enough to insure forward progress through the
		 * directory. Complete checks can be run by patching
		 * "dirchk" to be true.
		 */
		ep = (struct direct *)(bp->b_un.b_addr + entryoffsetinblock);
		if (ep->d_reclen == 0 ||
		    dirchk && ufs_dirbadentry(ep, entryoffsetinblock)) {
			int i;

			ufs_dirbad(dp, cnp->cn_ufs.ufs_offset, "mangled entry");
			i = DIRBLKSIZ - (entryoffsetinblock & (DIRBLKSIZ - 1));
			cnp->cn_ufs.ufs_offset += i;
			entryoffsetinblock += i;
			continue;
		}

		/*
		 * If an appropriate sized slot has not yet been found,
		 * check to see if one is available. Also accumulate space
		 * in the current block so that we can determine if
		 * compaction is viable.
		 */
		if (slotstatus != FOUND) {
			int size = ep->d_reclen;

			if (ep->d_ino != 0)
				size -= DIRSIZ(ep);
			if (size > 0) {
				if (size >= slotneeded) {
					slotstatus = FOUND;
					slotoffset = cnp->cn_ufs.ufs_offset;
					slotsize = ep->d_reclen;
				} else if (slotstatus == NONE) {
					slotfreespace += size;
					if (slotoffset == -1)
						slotoffset =
						      cnp->cn_ufs.ufs_offset;
					if (slotfreespace >= slotneeded) {
						slotstatus = COMPACT;
						slotsize =
						      cnp->cn_ufs.ufs_offset +
						      ep->d_reclen - slotoffset;
					}
				}
			}
		}

		/*
		 * Check for a name match.
		 */
		if (ep->d_ino) {
			if (ep->d_namlen == cnp->cn_namelen &&
			    !bcmp(cnp->cn_nameptr, ep->d_name,
				(unsigned)ep->d_namlen)) {
				/*
				 * Save directory entry's inode number and
				 * reclen in ndp->ni_ufs area, and release
				 * directory buffer.
				 */
				cnp->cn_ufs.ufs_ino = ep->d_ino;
				cnp->cn_ufs.ufs_reclen = ep->d_reclen;
				brelse(bp);
				goto found;
			}
		}
		prevoff = cnp->cn_ufs.ufs_offset;
		cnp->cn_ufs.ufs_offset += ep->d_reclen;
		entryoffsetinblock += ep->d_reclen;
		if (ep->d_ino)
			enduseful = cnp->cn_ufs.ufs_offset;
	}
/* notfound: */
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		cnp->cn_ufs.ufs_offset = 0;
		endsearch = dp->i_diroff;
		goto searchloop;
	}
	if (bp != NULL)
		brelse(bp);
	/*
	 * If creating, and at end of pathname and current
	 * directory has not been removed, then can consider
	 * allowing file to be created.
	 */
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
	    (cnp->cn_flags&ISLASTCN) && dp->i_nlink != 0) {
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		if (error = ufs_access(dvp, VWRITE, cnp->cn_cred, cnp->cn_proc))
			return (error);
		/*
		 * Return an indication of where the new directory
		 * entry should be put.  If we didn't find a slot,
		 * then set ndp->ni_ufs.ufs_count to 0 indicating
		 * that the new slot belongs at the end of the
		 * directory. If we found a slot, then the new entry
		 * can be put in the range from ndp->ni_ufs.ufs_offset
		 * to ndp->ni_ufs.ufs_offset + ndp->ni_ufs.ufs_count.
		 */
		if (slotstatus == NONE) {
			cnp->cn_ufs.ufs_offset = roundup(dp->i_size, DIRBLKSIZ);
			cnp->cn_ufs.ufs_count = 0;
			enduseful = cnp->cn_ufs.ufs_offset;
		} else {
			cnp->cn_ufs.ufs_offset = slotoffset;
			cnp->cn_ufs.ufs_count = slotsize;
			if (enduseful < slotoffset + slotsize)
				enduseful = slotoffset + slotsize;
		}
		cnp->cn_ufs.ufs_endoff = roundup(enduseful, DIRBLKSIZ);
		dp->i_flag |= IUPD|ICHG;
		/*
		 * We return with the directory locked, so that
		 * the parameters we set up above will still be
		 * valid if we actually decide to do a direnter().
		 * We return ni_vp == NULL to indicate that the entry
		 * does not currently exist; we leave a pointer to
		 * the (locked) directory inode in ndp->ni_dvp.
		 * The pathname buffer is saved so that the name
		 * can be obtained later.
		 *
		 * NB - if the directory is unlocked, then this
		 * information cannot be used.
		 */
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			IUNLOCK(dp);
	}
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if ((cnp->cn_flags&MAKEENTRY) && cnp->cn_nameiop != CREATE)
		cache_enter(dvp, *vpp, cnp);
	return (ENOENT);

found:
	if (numdirpasses == 2)
		nchstats.ncs_pass2++;
	/*
	 * Check that directory length properly reflects presence
	 * of this entry.
	 */
	if (entryoffsetinblock + DIRSIZ(ep) > dp->i_size) {
		ufs_dirbad(dp, cnp->cn_ufs.ufs_offset, "i_size too small");
		dp->i_size = entryoffsetinblock + DIRSIZ(ep);
		dp->i_flag |= IUPD|ICHG;
	}

	/*
	 * Found component in pathname.
	 * If the final component of path name, save information
	 * in the cache as to where the entry was found.
	 */
	if ((cnp->cn_flags&ISLASTCN) && cnp->cn_nameiop == LOOKUP)
		dp->i_diroff = cnp->cn_ufs.ufs_offset &~ (DIRBLKSIZ - 1);

	/*
	 * If deleting, and at end of pathname, return
	 * parameters which can be used to remove file.
	 * If the wantparent flag isn't set, we return only
	 * the directory (in ndp->ni_dvp), otherwise we go
	 * on and lock the inode, being careful with ".".
	 */
	if (cnp->cn_nameiop == DELETE && (cnp->cn_flags&ISLASTCN)) {
		/*
		 * Write access to directory required to delete files.
		 */
		if (error = ufs_access(dvp, VWRITE, cnp->cn_cred, cnp->cn_proc))
			return (error);
		/*
		 * Return pointer to current entry in cnp->cn_ufs.ufs_offset,
		 * and distance past previous entry (if there
		 * is a previous entry in this block) in ndp->ni_ufs.ufs_count.
		 * Save directory inode pointer in ndp->ni_dvp for dirremove().
		 */
		if ((cnp->cn_ufs.ufs_offset&(DIRBLKSIZ-1)) == 0)
			cnp->cn_ufs.ufs_count = 0;
		else
			cnp->cn_ufs.ufs_count =
			    cnp->cn_ufs.ufs_offset - prevoff;
		if (dp->i_number == cnp->cn_ufs.ufs_ino) {
			VREF(vdp);   /* NEEDSWORK: is vdp necessary? */
			*vpp = vdp;
			return (0);
		}
		if (error = VOP_VGET(dvp, cnp->cn_ufs.ufs_ino, &tdp))
			return (error);
		/*
		 * If directory is "sticky", then user must own
		 * the directory, or the file in it, else she
		 * may not delete it (unless she's root). This
		 * implements append-only directories.
		 */
		if ((dp->i_mode & ISVTX) &&
		    cnp->cn_cred->cr_uid != 0 &&
		    cnp->cn_cred->cr_uid != dp->i_uid &&
		    VTOI(tdp)->i_uid != cnp->cn_cred->cr_uid) {
			vput(tdp);
			return (EPERM);
		}
		*vpp = tdp;
		if (!lockparent)
			IUNLOCK(dp);
		return (0);
	}

	/*
	 * If rewriting (RENAME), return the inode and the
	 * information required to rewrite the present directory
	 * Must get inode of directory entry to verify it's a
	 * regular file, or empty directory.
	 */
	if (cnp->cn_nameiop == RENAME && wantparent && (cnp->cn_flags&ISLASTCN)) {
		if (error = ufs_access(dvp, VWRITE, cnp->cn_cred, cnp->cn_proc))
			return (error);
		/*
		 * Careful about locking second inode.
		 * This can only occur if the target is ".".
		 */
		if (dp->i_number == cnp->cn_ufs.ufs_ino)
			return (EISDIR);
		if (error = VOP_VGET(dvp, cnp->cn_ufs.ufs_ino, &tdp))
			return (error);
		*vpp = tdp;
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			IUNLOCK(dp);
		return (0);
	}

	/*
	 * Step through the translation in the name.  We do not `iput' the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "pdp".  We must get the target inode before unlocking
	 * the directory to insure that the inode will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * inodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the `iget' for the
	 * inode associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
	pdp = dp;
	if (cnp->cn_flags&ISDOTDOT) {
		IUNLOCK(pdp);	/* race to get the inode */
		if (error = VOP_VGET(dvp, cnp->cn_ufs.ufs_ino, &tdp)) {
			ILOCK(pdp);
			return (error);
		}
		if (lockparent && (cnp->cn_flags&ISLASTCN))
			ILOCK(pdp);
		*vpp = tdp;
	} else if (dp->i_number == cnp->cn_ufs.ufs_ino) {
		VREF(dvp);	/* we want ourself, ie "." */
		*vpp = dvp;
	} else {
		if (error = VOP_VGET(dvp, cnp->cn_ufs.ufs_ino, &tdp))
			return (error);
		if (!lockparent || !(cnp->cn_flags&ISLASTCN))
			IUNLOCK(pdp);
		*vpp = tdp;
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, *vpp, cnp);
	return (0);
}

void
ufs_dirbad(ip, offset, how)
	struct inode *ip;
	off_t offset;
	char *how;
{
	struct mount *mp;

	mp = ITOV(ip)->v_mount;
	(void)printf("%s: bad dir ino %d at offset %d: %s\n",
	    mp->mnt_stat.f_mntonname, ip->i_number, offset, how);
	if ((mp->mnt_stat.f_flags & MNT_RDONLY) == 0)
		panic("bad dir");
}

/*
 * Do consistency checking on a directory entry:
 *	record length must be multiple of 4
 *	entry must fit in rest of its DIRBLKSIZ block
 *	record must be large enough to contain entry
 *	name is not longer than MAXNAMLEN
 *	name must be as long as advertised, and null terminated
 */
int
ufs_dirbadentry(ep, entryoffsetinblock)
	register struct direct *ep;
	int entryoffsetinblock;
{
	register int i;

	if ((ep->d_reclen & 0x3) != 0 ||
	    ep->d_reclen > DIRBLKSIZ - (entryoffsetinblock & (DIRBLKSIZ - 1)) ||
	    ep->d_reclen < DIRSIZ(ep) || ep->d_namlen > MAXNAMLEN) {
		/*return (1); */
		printf("First bad\n");
		goto bad;
	}
	for (i = 0; i < ep->d_namlen; i++)
		if (ep->d_name[i] == '\0') {
			/*return (1); */
			printf("Second bad\n");
			goto bad;
	}
	if (ep->d_name[i])
		goto bad;
	return (ep->d_name[i]);
bad:
printf("ufs_dirbadentry: jumping out: reclen: %d namlen %d ino %d name %s\n",
	ep->d_reclen, ep->d_namlen, ep->d_ino, ep->d_name );
	return(1);
}

/*
 * Write a directory entry after a call to namei, using the parameters
 * that it left in nameidata.  The argument ip is the inode which the new
 * directory entry will refer to.  The nameidata field ndp->ni_dvp is a
 * pointer to the directory to be written, which was left locked by namei.
 * Remaining parameters (ndp->ni_ufs.ufs_offset, ndp->ni_ufs.ufs_count)
 * indicate how the space for the new entry is to be obtained.
 */
int
ufs_direnter(ip, dvp, cnp)   /* converted to CN.  */
/* old: ufs_direnter(ip, ndp) */
	struct inode *ip;
	struct vnode *dvp;
	register struct componentname *cnp;
{
	register struct direct *ep, *nep;
	register struct inode *dp;
	struct buf *bp;
	struct direct newdir;
	struct iovec aiov;
	struct uio auio;
	u_int dsize;
	int error, loc, newentrysize, spacefree;
	char *dirbuf;

#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & SAVENAME) == 0)
		panic("direnter: missing name");
#endif
	dp = VTOI(dvp);
	newdir.d_ino = ip->i_number;
	newdir.d_namlen = cnp->cn_namelen;
	bcopy(cnp->cn_nameptr, newdir.d_name, (unsigned)cnp->cn_namelen + 1);
	newentrysize = DIRSIZ(&newdir);
	if (cnp->cn_ufs.ufs_count == 0) {
		/*
		 * If cnp->cn_ufs.ufs_count is 0, then namei could find no
		 * space in the directory. Here, ndp->ni_ufs.ufs_offset will
		 * be on a directory block boundary and we will write the
		 * new entry into a fresh block.
		 */
		if (cnp->cn_ufs.ufs_offset & (DIRBLKSIZ - 1))
			panic("wdir: newblk");
		auio.uio_offset = cnp->cn_ufs.ufs_offset;
		newdir.d_reclen = DIRBLKSIZ;
		auio.uio_resid = newentrysize;
		aiov.iov_len = newentrysize;
		aiov.iov_base = (caddr_t)&newdir;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_rw = UIO_WRITE;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_procp = (struct proc *)0;
		error = VOP_WRITE(dvp, &auio, IO_SYNC, cnp->cn_cred);
		if (DIRBLKSIZ >
		    VFSTOUFS(dvp->v_mount)->um_mountp->mnt_stat.f_bsize)
			/* XXX should grow with balloc() */
			panic("ufs_direnter: frag size");
		else if (!error) {
			dp->i_size = roundup(dp->i_size, DIRBLKSIZ);
			dp->i_flag |= ICHG;
		}
		return (error);
	}

	/*
	 * If cnp->cn_ufs.ufs_count is non-zero, then namei found space
	 * for the new entry in the range cnp->cn_ufs.ufs_offset to
	 * cnp->cn_ufs.ufs_offset + cnp->cn_ufs.ufs_count in the directory.
	 * To use this space, we may have to compact the entries located
	 * there, by copying them together towards the beginning of the
	 * block, leaving the free space in one usable chunk at the end.
	 */

	/*
	 * Increase size of directory if entry eats into new space.
	 * This should never push the size past a new multiple of
	 * DIRBLKSIZE.
	 *
	 * N.B. - THIS IS AN ARTIFACT OF 4.2 AND SHOULD NEVER HAPPEN.
	 */
	if (cnp->cn_ufs.ufs_offset + cnp->cn_ufs.ufs_count > dp->i_size)
		dp->i_size = cnp->cn_ufs.ufs_offset + cnp->cn_ufs.ufs_count;
	/*
	 * Get the block containing the space for the new directory entry.
	 */
	if (error = VOP_BLKATOFF(dvp, cnp->cn_ufs.ufs_offset, &dirbuf, &bp))
		return (error);
	/*
	 * Find space for the new entry. In the simple case, the entry at
	 * offset base will have the space. If it does not, then namei
	 * arranged that compacting the region cnp->cn_ufs.ufs_offset to
	 * cnp->cn_ufs.ufs_offset + cnp->cn_ufs.ufs_count would yield the
	 * space.
	 */
	ep = (struct direct *)dirbuf;
	dsize = DIRSIZ(ep);
	spacefree = ep->d_reclen - dsize;
	for (loc = ep->d_reclen; loc < cnp->cn_ufs.ufs_count; ) {
		nep = (struct direct *)(dirbuf + loc);
		if (ep->d_ino) {
			/* trim the existing slot */
			ep->d_reclen = dsize;
			ep = (struct direct *)((char *)ep + dsize);
		} else {
			/* overwrite; nothing there; header is ours */
			spacefree += dsize;
		}
		dsize = DIRSIZ(nep);
		spacefree += nep->d_reclen - dsize;
		loc += nep->d_reclen;
		bcopy((caddr_t)nep, (caddr_t)ep, dsize);
	}
	/*
	 * Update the pointer fields in the previous entry (if any),
	 * copy in the new entry, and write out the block.
	 */
	if (ep->d_ino == 0) {
		if (spacefree + dsize < newentrysize)
			panic("wdir: compact1");
		newdir.d_reclen = spacefree + dsize;
	} else {
		if (spacefree < newentrysize)
			panic("wdir: compact2");
		newdir.d_reclen = spacefree;
		ep->d_reclen = dsize;
		ep = (struct direct *)((char *)ep + dsize);
	}
	bcopy((caddr_t)&newdir, (caddr_t)ep, (u_int)newentrysize);
	error = VOP_BWRITE(bp);
	dp->i_flag |= IUPD|ICHG;
	if (!error && cnp->cn_ufs.ufs_endoff &&
	    cnp->cn_ufs.ufs_endoff < dp->i_size)
		error = VOP_TRUNCATE(dvp, (u_long)cnp->cn_ufs.ufs_endoff,
		    IO_SYNC);
	return (error);
}

/*
 * Remove a directory entry after a call to namei, using
 * the parameters which it left in nameidata. The entry
 * ni_ufs.ufs_offset contains the offset into the directory of the
 * entry to be eliminated.  The ni_ufs.ufs_count field contains the
 * size of the previous record in the directory.  If this
 * is 0, the first entry is being deleted, so we need only
 * zero the inode number to mark the entry as free.  If the
 * entry is not the first in the directory, we must reclaim
 * the space of the now empty record by adding the record size
 * to the size of the previous entry.
 */
int
ufs_dirremove(dvp, cnp)   /* converted to CN.  */
	struct vnode *dvp;
	struct componentname *cnp;
/* old: ufs_dirremove(ndp) */
{
	register struct inode *dp;
	struct direct *ep;
	struct buf *bp;
	int error;

	dp = VTOI(dvp);
	if (cnp->cn_ufs.ufs_count == 0) {
		/*
		 * First entry in block: set d_ino to zero.
		 */
		if (error = VOP_BLKATOFF(dvp, cnp->cn_ufs.ufs_offset,
		    (char **)&ep, &bp))
			return (error);
		ep->d_ino = 0;
		error = VOP_BWRITE(bp);
		dp->i_flag |= IUPD|ICHG;
		return (error);
	}
	/*
	 * Collapse new free space into previous entry.
	 */
	if (error = VOP_BLKATOFF(dvp,
	    cnp->cn_ufs.ufs_offset - cnp->cn_ufs.ufs_count, (char **)&ep, &bp))
		return (error);
	ep->d_reclen += cnp->cn_ufs.ufs_reclen;
	error = VOP_BWRITE(bp);
	dp->i_flag |= IUPD|ICHG;
	return (error);
}

/*
 * Rewrite an existing directory entry to point at the inode
 * supplied.  The parameters describing the directory entry are
 * set up by a call to namei.
 */
int
ufs_dirrewrite(dp, ip, cnp)
/* old: ufs_dirrewrite(dp, ip, ndp) */
	struct inode *dp, *ip;
	struct componentname *cnp;
{
	struct buf *bp;
	struct direct *ep;
	int error;

	if (error = VOP_BLKATOFF(ITOV(dp), cnp->cn_ufs.ufs_offset,
	    (char **)&ep, &bp))
		return (error);
	ep->d_ino = ip->i_number;
	error = VOP_BWRITE(bp);
	dp->i_flag |= IUPD|ICHG;
	return (error);
}

/*
 * Check if a directory is empty or not.
 * Inode supplied must be locked.
 *
 * Using a struct dirtemplate here is not precisely
 * what we want, but better than using a struct direct.
 *
 * NB: does not handle corrupted directories.
 */
int
ufs_dirempty(ip, parentino, cred)
	register struct inode *ip;
	ino_t parentino;
	struct ucred *cred;
{
	register off_t off;
	struct dirtemplate dbuf;
	register struct direct *dp = (struct direct *)&dbuf;
	int error, count;
#define	MINDIRSIZ (sizeof (struct dirtemplate) / 2)

	for (off = 0; off < ip->i_size; off += dp->d_reclen) {
		error = vn_rdwr(UIO_READ, ITOV(ip), (caddr_t)dp, MINDIRSIZ, off,
		   UIO_SYSSPACE, IO_NODELOCKED, cred, &count, (struct proc *)0);
		/*
		 * Since we read MINDIRSIZ, residual must
		 * be 0 unless we're at end of file.
		 */
		if (error || count != 0)
			return (0);
		/* avoid infinite loops */
		if (dp->d_reclen == 0)
			return (0);
		/* skip empty entries */
		if (dp->d_ino == 0)
			continue;
		/* accept only "." and ".." */
		if (dp->d_namlen > 2)
			return (0);
		if (dp->d_name[0] != '.')
			return (0);
		/*
		 * At this point d_namlen must be 1 or 2.
		 * 1 implies ".", 2 implies ".." if second
		 * char is also "."
		 */
		if (dp->d_namlen == 1)
			continue;
		if (dp->d_name[1] == '.' && dp->d_ino == parentino)
			continue;
		return (0);
	}
	return (1);
}

/*
 * Check if source directory is in the path of the target directory.
 * Target is supplied locked, source is unlocked.
 * The target is always iput before returning.
 */
int
ufs_checkpath(source, target, cred)
	struct inode *source, *target;
	struct ucred *cred;
{
	struct dirtemplate dirbuf;
	register struct inode *ip;
	struct vnode *vp;
	int error, rootino;

	ip = target;
	if (ip->i_number == source->i_number) {
		error = EEXIST;
		goto out;
	}
	rootino = ROOTINO;
	error = 0;
	if (ip->i_number == rootino)
		goto out;

	for (;;) {
		if ((ip->i_mode&IFMT) != IFDIR) {
			error = ENOTDIR;
			break;
		}
		vp = ITOV(ip);
		error = vn_rdwr(UIO_READ, vp, (caddr_t)&dirbuf,
			sizeof (struct dirtemplate), (off_t)0, UIO_SYSSPACE,
			IO_NODELOCKED, cred, (int *)0, (struct proc *)0);
		if (error != 0)
			break;
		if (dirbuf.dotdot_namlen != 2 ||
		    dirbuf.dotdot_name[0] != '.' ||
		    dirbuf.dotdot_name[1] != '.') {
			error = ENOTDIR;
			break;
		}
		if (dirbuf.dotdot_ino == source->i_number) {
			error = EINVAL;
			break;
		}
		if (dirbuf.dotdot_ino == rootino)
			break;
		ufs_iput(ip);
		if (error = VOP_VGET(vp, dirbuf.dotdot_ino, &vp))
			break;
		ip = VTOI(vp);
	}

out:
	if (error == ENOTDIR)
		printf("checkpath: .. not a directory\n");
	if (ip != NULL)
		ufs_iput(ip);
	return (error);
}
