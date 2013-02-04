/*	$NetBSD: tmpfs_subr.c,v 1.35 2007/07/09 21:10:50 ad Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Efficient memory file system supporting functions.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fnv_hash.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

#include <fs/tmpfs/tmpfs.h>
#include <fs/tmpfs/tmpfs_fifoops.h>
#include <fs/tmpfs/tmpfs_vnops.h>

struct tmpfs_dir_cursor {
	struct tmpfs_dirent	*tdc_current;
	struct tmpfs_dirent	*tdc_tree;
};

SYSCTL_NODE(_vfs, OID_AUTO, tmpfs, CTLFLAG_RW, 0, "tmpfs file system");

static long tmpfs_pages_reserved = TMPFS_PAGES_MINRESERVED;

static int
sysctl_mem_reserved(SYSCTL_HANDLER_ARGS)
{
	int error;
	long pages, bytes;

	pages = *(long *)arg1;
	bytes = pages * PAGE_SIZE;

	error = sysctl_handle_long(oidp, &bytes, 0, req);
	if (error || !req->newptr)
		return (error);

	pages = bytes / PAGE_SIZE;
	if (pages < TMPFS_PAGES_MINRESERVED)
		return (EINVAL);

	*(long *)arg1 = pages;
	return (0);
}

SYSCTL_PROC(_vfs_tmpfs, OID_AUTO, memory_reserved, CTLTYPE_LONG|CTLFLAG_RW,
    &tmpfs_pages_reserved, 0, sysctl_mem_reserved, "L",
    "Amount of available memory and swap below which tmpfs growth stops");

static __inline int tmpfs_dirtree_cmp(struct tmpfs_dirent *a,
    struct tmpfs_dirent *b);
RB_PROTOTYPE_STATIC(tmpfs_dir, tmpfs_dirent, uh.td_entries, tmpfs_dirtree_cmp);

size_t
tmpfs_mem_avail(void)
{
	vm_ooffset_t avail;

	avail = swap_pager_avail + cnt.v_free_count + cnt.v_cache_count -
	    tmpfs_pages_reserved;
	if (__predict_false(avail < 0))
		avail = 0;
	return (avail);
}

size_t
tmpfs_pages_used(struct tmpfs_mount *tmp)
{
	const size_t node_size = sizeof(struct tmpfs_node) +
	    sizeof(struct tmpfs_dirent);
	size_t meta_pages;

	meta_pages = howmany((uintmax_t)tmp->tm_nodes_inuse * node_size,
	    PAGE_SIZE);
	return (meta_pages + tmp->tm_pages_used);
}

static size_t
tmpfs_pages_check_avail(struct tmpfs_mount *tmp, size_t req_pages)
{
	if (tmpfs_mem_avail() < req_pages)
		return (0);

	if (tmp->tm_pages_max != SIZE_MAX &&
	    tmp->tm_pages_max < req_pages + tmpfs_pages_used(tmp))
			return (0);

	return (1);
}

/* --------------------------------------------------------------------- */

/*
 * Allocates a new node of type 'type' inside the 'tmp' mount point, with
 * its owner set to 'uid', its group to 'gid' and its mode set to 'mode',
 * using the credentials of the process 'p'.
 *
 * If the node type is set to 'VDIR', then the parent parameter must point
 * to the parent directory of the node being created.  It may only be NULL
 * while allocating the root node.
 *
 * If the node type is set to 'VBLK' or 'VCHR', then the rdev parameter
 * specifies the device the node represents.
 *
 * If the node type is set to 'VLNK', then the parameter target specifies
 * the file name of the target file for the symbolic link that is being
 * created.
 *
 * Note that new nodes are retrieved from the available list if it has
 * items or, if it is empty, from the node pool as long as there is enough
 * space to create them.
 *
 * Returns zero on success or an appropriate error code on failure.
 */
int
tmpfs_alloc_node(struct tmpfs_mount *tmp, enum vtype type,
    uid_t uid, gid_t gid, mode_t mode, struct tmpfs_node *parent,
    char *target, dev_t rdev, struct tmpfs_node **node)
{
	struct tmpfs_node *nnode;

	/* If the root directory of the 'tmp' file system is not yet
	 * allocated, this must be the request to do it. */
	MPASS(IMPLIES(tmp->tm_root == NULL, parent == NULL && type == VDIR));

	MPASS(IFF(type == VLNK, target != NULL));
	MPASS(IFF(type == VBLK || type == VCHR, rdev != VNOVAL));

	if (tmp->tm_nodes_inuse >= tmp->tm_nodes_max)
		return (ENOSPC);
	if (tmpfs_pages_check_avail(tmp, 1) == 0)
		return (ENOSPC);

	nnode = (struct tmpfs_node *)uma_zalloc_arg(
				tmp->tm_node_pool, tmp, M_WAITOK);

	/* Generic initialization. */
	nnode->tn_type = type;
	vfs_timestamp(&nnode->tn_atime);
	nnode->tn_birthtime = nnode->tn_ctime = nnode->tn_mtime =
	    nnode->tn_atime;
	nnode->tn_uid = uid;
	nnode->tn_gid = gid;
	nnode->tn_mode = mode;
	nnode->tn_id = alloc_unr(tmp->tm_ino_unr);

	/* Type-specific initialization. */
	switch (nnode->tn_type) {
	case VBLK:
	case VCHR:
		nnode->tn_rdev = rdev;
		break;

	case VDIR:
		RB_INIT(&nnode->tn_dir.tn_dirhead);
		LIST_INIT(&nnode->tn_dir.tn_dupindex);
		MPASS(parent != nnode);
		MPASS(IMPLIES(parent == NULL, tmp->tm_root == NULL));
		nnode->tn_dir.tn_parent = (parent == NULL) ? nnode : parent;
		nnode->tn_dir.tn_readdir_lastn = 0;
		nnode->tn_dir.tn_readdir_lastp = NULL;
		nnode->tn_links++;
		TMPFS_NODE_LOCK(nnode->tn_dir.tn_parent);
		nnode->tn_dir.tn_parent->tn_links++;
		TMPFS_NODE_UNLOCK(nnode->tn_dir.tn_parent);
		break;

	case VFIFO:
		/* FALLTHROUGH */
	case VSOCK:
		break;

	case VLNK:
		MPASS(strlen(target) < MAXPATHLEN);
		nnode->tn_size = strlen(target);
		nnode->tn_link = malloc(nnode->tn_size, M_TMPFSNAME,
		    M_WAITOK);
		memcpy(nnode->tn_link, target, nnode->tn_size);
		break;

	case VREG:
		nnode->tn_reg.tn_aobj =
		    vm_pager_allocate(OBJT_SWAP, NULL, 0, VM_PROT_DEFAULT, 0,
			NULL /* XXXKIB - tmpfs needs swap reservation */);
		break;

	default:
		panic("tmpfs_alloc_node: type %p %d", nnode, (int)nnode->tn_type);
	}

	TMPFS_LOCK(tmp);
	LIST_INSERT_HEAD(&tmp->tm_nodes_used, nnode, tn_entries);
	tmp->tm_nodes_inuse++;
	TMPFS_UNLOCK(tmp);

	*node = nnode;
	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Destroys the node pointed to by node from the file system 'tmp'.
 * If the node does not belong to the given mount point, the results are
 * unpredicted.
 *
 * If the node references a directory; no entries are allowed because
 * their removal could need a recursive algorithm, something forbidden in
 * kernel space.  Furthermore, there is not need to provide such
 * functionality (recursive removal) because the only primitives offered
 * to the user are the removal of empty directories and the deletion of
 * individual files.
 *
 * Note that nodes are not really deleted; in fact, when a node has been
 * allocated, it cannot be deleted during the whole life of the file
 * system.  Instead, they are moved to the available list and remain there
 * until reused.
 */
void
tmpfs_free_node(struct tmpfs_mount *tmp, struct tmpfs_node *node)
{
	vm_object_t uobj;

#ifdef INVARIANTS
	TMPFS_NODE_LOCK(node);
	MPASS(node->tn_vnode == NULL);
	MPASS((node->tn_vpstate & TMPFS_VNODE_ALLOCATING) == 0);
	TMPFS_NODE_UNLOCK(node);
#endif

	TMPFS_LOCK(tmp);
	LIST_REMOVE(node, tn_entries);
	tmp->tm_nodes_inuse--;
	TMPFS_UNLOCK(tmp);

	switch (node->tn_type) {
	case VNON:
		/* Do not do anything.  VNON is provided to let the
		 * allocation routine clean itself easily by avoiding
		 * duplicating code in it. */
		/* FALLTHROUGH */
	case VBLK:
		/* FALLTHROUGH */
	case VCHR:
		/* FALLTHROUGH */
	case VDIR:
		/* FALLTHROUGH */
	case VFIFO:
		/* FALLTHROUGH */
	case VSOCK:
		break;

	case VLNK:
		free(node->tn_link, M_TMPFSNAME);
		break;

	case VREG:
		uobj = node->tn_reg.tn_aobj;
		if (uobj != NULL) {
			TMPFS_LOCK(tmp);
			tmp->tm_pages_used -= uobj->size;
			TMPFS_UNLOCK(tmp);
			vm_object_deallocate(uobj);
		}
		break;

	default:
		panic("tmpfs_free_node: type %p %d", node, (int)node->tn_type);
	}

	free_unr(tmp->tm_ino_unr, node->tn_id);
	uma_zfree(tmp->tm_node_pool, node);
}

/* --------------------------------------------------------------------- */

static __inline uint32_t
tmpfs_dirent_hash(const char *name, u_int len)
{
	uint32_t hash;

	hash = fnv_32_buf(name, len, FNV1_32_INIT + len) & TMPFS_DIRCOOKIE_MASK;
#ifdef TMPFS_DEBUG_DIRCOOKIE_DUP
	hash &= 0xf;
#endif
	if (hash < TMPFS_DIRCOOKIE_MIN)
		hash += TMPFS_DIRCOOKIE_MIN;

	return (hash);
}

static __inline off_t
tmpfs_dirent_cookie(struct tmpfs_dirent *de)
{
	MPASS(de->td_cookie >= TMPFS_DIRCOOKIE_MIN);

	return (de->td_cookie);
}

static __inline boolean_t
tmpfs_dirent_dup(struct tmpfs_dirent *de)
{
	return ((de->td_cookie & TMPFS_DIRCOOKIE_DUP) != 0);
}

static __inline boolean_t
tmpfs_dirent_duphead(struct tmpfs_dirent *de)
{
	return ((de->td_cookie & TMPFS_DIRCOOKIE_DUPHEAD) != 0);
}

void
tmpfs_dirent_init(struct tmpfs_dirent *de, const char *name, u_int namelen)
{
	de->td_hash = de->td_cookie = tmpfs_dirent_hash(name, namelen);
	memcpy(de->ud.td_name, name, namelen);
	de->td_namelen = namelen;
}

/*
 * Allocates a new directory entry for the node node with a name of name.
 * The new directory entry is returned in *de.
 *
 * The link count of node is increased by one to reflect the new object
 * referencing it.
 *
 * Returns zero on success or an appropriate error code on failure.
 */
int
tmpfs_alloc_dirent(struct tmpfs_mount *tmp, struct tmpfs_node *node,
    const char *name, u_int len, struct tmpfs_dirent **de)
{
	struct tmpfs_dirent *nde;

	nde = uma_zalloc(tmp->tm_dirent_pool, M_WAITOK);
	nde->td_node = node;
	if (name != NULL) {
		nde->ud.td_name = malloc(len, M_TMPFSNAME, M_WAITOK);
		tmpfs_dirent_init(nde, name, len);
	} else
		nde->td_namelen = 0;
	if (node != NULL)
		node->tn_links++;

	*de = nde;

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Frees a directory entry.  It is the caller's responsibility to destroy
 * the node referenced by it if needed.
 *
 * The link count of node is decreased by one to reflect the removal of an
 * object that referenced it.  This only happens if 'node_exists' is true;
 * otherwise the function will not access the node referred to by the
 * directory entry, as it may already have been released from the outside.
 */
void
tmpfs_free_dirent(struct tmpfs_mount *tmp, struct tmpfs_dirent *de)
{
	struct tmpfs_node *node;

	node = de->td_node;
	if (node != NULL) {
		MPASS(node->tn_links > 0);
		node->tn_links--;
	}
	if (!tmpfs_dirent_duphead(de) && de->ud.td_name != NULL)
		free(de->ud.td_name, M_TMPFSNAME);
	uma_zfree(tmp->tm_dirent_pool, de);
}

/* --------------------------------------------------------------------- */

/*
 * Allocates a new vnode for the node node or returns a new reference to
 * an existing one if the node had already a vnode referencing it.  The
 * resulting locked vnode is returned in *vpp.
 *
 * Returns zero on success or an appropriate error code on failure.
 */
int
tmpfs_alloc_vp(struct mount *mp, struct tmpfs_node *node, int lkflag,
    struct vnode **vpp)
{
	int error = 0;
	struct vnode *vp;

loop:
	TMPFS_NODE_LOCK(node);
	if ((vp = node->tn_vnode) != NULL) {
		MPASS((node->tn_vpstate & TMPFS_VNODE_DOOMED) == 0);
		VI_LOCK(vp);
		TMPFS_NODE_UNLOCK(node);
		error = vget(vp, lkflag | LK_INTERLOCK, curthread);
		if (error != 0) {
			vp = NULL;
			goto out;
		}

		/*
		 * Make sure the vnode is still there after
		 * getting the interlock to avoid racing a free.
		 */
		if (node->tn_vnode == NULL || node->tn_vnode != vp) {
			vput(vp);
			goto loop;
		}

		goto out;
	}

	if ((node->tn_vpstate & TMPFS_VNODE_DOOMED) ||
	    (node->tn_type == VDIR && node->tn_dir.tn_parent == NULL)) {
		TMPFS_NODE_UNLOCK(node);
		error = ENOENT;
		vp = NULL;
		goto out;
	}

	/*
	 * otherwise lock the vp list while we call getnewvnode
	 * since that can block.
	 */
	if (node->tn_vpstate & TMPFS_VNODE_ALLOCATING) {
		node->tn_vpstate |= TMPFS_VNODE_WANT;
		error = msleep((caddr_t) &node->tn_vpstate,
		    TMPFS_NODE_MTX(node), PDROP | PCATCH,
		    "tmpfs_alloc_vp", 0);
		if (error)
			return error;

		goto loop;
	} else
		node->tn_vpstate |= TMPFS_VNODE_ALLOCATING;
	
	TMPFS_NODE_UNLOCK(node);

	/* Get a new vnode and associate it with our node. */
	error = getnewvnode("tmpfs", mp, &tmpfs_vnodeop_entries, &vp);
	if (error != 0)
		goto unlock;
	MPASS(vp != NULL);

	(void) vn_lock(vp, lkflag | LK_RETRY);

	vp->v_data = node;
	vp->v_type = node->tn_type;

	/* Type-specific initialization. */
	switch (node->tn_type) {
	case VBLK:
		/* FALLTHROUGH */
	case VCHR:
		/* FALLTHROUGH */
	case VLNK:
		/* FALLTHROUGH */
	case VREG:
		/* FALLTHROUGH */
	case VSOCK:
		break;
	case VFIFO:
		vp->v_op = &tmpfs_fifoop_entries;
		break;
	case VDIR:
		MPASS(node->tn_dir.tn_parent != NULL);
		if (node->tn_dir.tn_parent == node)
			vp->v_vflag |= VV_ROOT;
		break;

	default:
		panic("tmpfs_alloc_vp: type %p %d", node, (int)node->tn_type);
	}

	vnode_pager_setsize(vp, node->tn_size);
	error = insmntque(vp, mp);
	if (error)
		vp = NULL;

unlock:
	TMPFS_NODE_LOCK(node);

	MPASS(node->tn_vpstate & TMPFS_VNODE_ALLOCATING);
	node->tn_vpstate &= ~TMPFS_VNODE_ALLOCATING;
	node->tn_vnode = vp;

	if (node->tn_vpstate & TMPFS_VNODE_WANT) {
		node->tn_vpstate &= ~TMPFS_VNODE_WANT;
		TMPFS_NODE_UNLOCK(node);
		wakeup((caddr_t) &node->tn_vpstate);
	} else
		TMPFS_NODE_UNLOCK(node);

out:
	*vpp = vp;

#ifdef INVARIANTS
	if (error == 0) {
		MPASS(*vpp != NULL && VOP_ISLOCKED(*vpp));
		TMPFS_NODE_LOCK(node);
		MPASS(*vpp == node->tn_vnode);
		TMPFS_NODE_UNLOCK(node);
	}
#endif

	return error;
}

/* --------------------------------------------------------------------- */

/*
 * Destroys the association between the vnode vp and the node it
 * references.
 */
void
tmpfs_free_vp(struct vnode *vp)
{
	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);

	mtx_assert(TMPFS_NODE_MTX(node), MA_OWNED);
	node->tn_vnode = NULL;
	vp->v_data = NULL;
}

/* --------------------------------------------------------------------- */

/*
 * Allocates a new file of type 'type' and adds it to the parent directory
 * 'dvp'; this addition is done using the component name given in 'cnp'.
 * The ownership of the new file is automatically assigned based on the
 * credentials of the caller (through 'cnp'), the group is set based on
 * the parent directory and the mode is determined from the 'vap' argument.
 * If successful, *vpp holds a vnode to the newly created file and zero
 * is returned.  Otherwise *vpp is NULL and the function returns an
 * appropriate error code.
 */
int
tmpfs_alloc_file(struct vnode *dvp, struct vnode **vpp, struct vattr *vap,
    struct componentname *cnp, char *target)
{
	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *dnode;
	struct tmpfs_node *node;
	struct tmpfs_node *parent;

	MPASS(VOP_ISLOCKED(dvp));
	MPASS(cnp->cn_flags & HASBUF);

	tmp = VFS_TO_TMPFS(dvp->v_mount);
	dnode = VP_TO_TMPFS_DIR(dvp);
	*vpp = NULL;

	/* If the entry we are creating is a directory, we cannot overflow
	 * the number of links of its parent, because it will get a new
	 * link. */
	if (vap->va_type == VDIR) {
		/* Ensure that we do not overflow the maximum number of links
		 * imposed by the system. */
		MPASS(dnode->tn_links <= LINK_MAX);
		if (dnode->tn_links == LINK_MAX) {
			error = EMLINK;
			goto out;
		}

		parent = dnode;
		MPASS(parent != NULL);
	} else
		parent = NULL;

	/* Allocate a node that represents the new file. */
	error = tmpfs_alloc_node(tmp, vap->va_type, cnp->cn_cred->cr_uid,
	    dnode->tn_gid, vap->va_mode, parent, target, vap->va_rdev, &node);
	if (error != 0)
		goto out;

	/* Allocate a directory entry that points to the new file. */
	error = tmpfs_alloc_dirent(tmp, node, cnp->cn_nameptr, cnp->cn_namelen,
	    &de);
	if (error != 0) {
		tmpfs_free_node(tmp, node);
		goto out;
	}

	/* Allocate a vnode for the new file. */
	error = tmpfs_alloc_vp(dvp->v_mount, node, LK_EXCLUSIVE, vpp);
	if (error != 0) {
		tmpfs_free_dirent(tmp, de);
		tmpfs_free_node(tmp, node);
		goto out;
	}

	/* Now that all required items are allocated, we can proceed to
	 * insert the new node into the directory, an operation that
	 * cannot fail. */
	if (cnp->cn_flags & ISWHITEOUT)
		tmpfs_dir_whiteout_remove(dvp, cnp);
	tmpfs_dir_attach(dvp, de);

out:

	return error;
}

/* --------------------------------------------------------------------- */

static struct tmpfs_dirent *
tmpfs_dir_first(struct tmpfs_node *dnode, struct tmpfs_dir_cursor *dc)
{
	struct tmpfs_dirent *de;

	de = RB_MIN(tmpfs_dir, &dnode->tn_dir.tn_dirhead);
	dc->tdc_tree = de;
	if (de != NULL && tmpfs_dirent_duphead(de))
		de = LIST_FIRST(&de->ud.td_duphead);
	dc->tdc_current = de;

	return (dc->tdc_current);
}

static struct tmpfs_dirent *
tmpfs_dir_next(struct tmpfs_node *dnode, struct tmpfs_dir_cursor *dc)
{
	struct tmpfs_dirent *de;

	MPASS(dc->tdc_tree != NULL);
	if (tmpfs_dirent_dup(dc->tdc_current)) {
		dc->tdc_current = LIST_NEXT(dc->tdc_current, uh.td_dup.entries);
		if (dc->tdc_current != NULL)
			return (dc->tdc_current);
	}
	dc->tdc_tree = dc->tdc_current = RB_NEXT(tmpfs_dir,
	    &dnode->tn_dir.tn_dirhead, dc->tdc_tree);
	if ((de = dc->tdc_current) != NULL && tmpfs_dirent_duphead(de)) {
		dc->tdc_current = LIST_FIRST(&de->ud.td_duphead);
		MPASS(dc->tdc_current != NULL);
	}

	return (dc->tdc_current);
}

/* Lookup directory entry in RB-Tree. Function may return duphead entry. */
static struct tmpfs_dirent *
tmpfs_dir_xlookup_hash(struct tmpfs_node *dnode, uint32_t hash)
{
	struct tmpfs_dirent *de, dekey;

	dekey.td_hash = hash;
	de = RB_FIND(tmpfs_dir, &dnode->tn_dir.tn_dirhead, &dekey);
	return (de);
}

/* Lookup directory entry by cookie, initialize directory cursor accordingly. */
static struct tmpfs_dirent *
tmpfs_dir_lookup_cookie(struct tmpfs_node *node, off_t cookie,
    struct tmpfs_dir_cursor *dc)
{
	struct tmpfs_dir *dirhead = &node->tn_dir.tn_dirhead;
	struct tmpfs_dirent *de, dekey;

	MPASS(cookie >= TMPFS_DIRCOOKIE_MIN);

	if (cookie == node->tn_dir.tn_readdir_lastn &&
	    (de = node->tn_dir.tn_readdir_lastp) != NULL) {
		/* Protect against possible race, tn_readdir_last[pn]
		 * may be updated with only shared vnode lock held. */
		if (cookie == tmpfs_dirent_cookie(de))
			goto out;
	}

	if ((cookie & TMPFS_DIRCOOKIE_DUP) != 0) {
		LIST_FOREACH(de, &node->tn_dir.tn_dupindex,
		    uh.td_dup.index_entries) {
			MPASS(tmpfs_dirent_dup(de));
			if (de->td_cookie == cookie)
				goto out;
			/* dupindex list is sorted. */
			if (de->td_cookie < cookie) {
				de = NULL;
				goto out;
			}
		}
		MPASS(de == NULL);
		goto out;
	}

	MPASS((cookie & TMPFS_DIRCOOKIE_MASK) == cookie);
	dekey.td_hash = cookie;
	/* Recover if direntry for cookie was removed */
	de = RB_NFIND(tmpfs_dir, dirhead, &dekey);
	dc->tdc_tree = de;
	dc->tdc_current = de;
	if (de != NULL && tmpfs_dirent_duphead(de)) {
		dc->tdc_current = LIST_FIRST(&de->ud.td_duphead);
		MPASS(dc->tdc_current != NULL);
	}
	return (dc->tdc_current);

out:
	dc->tdc_tree = de;
	dc->tdc_current = de;
	if (de != NULL && tmpfs_dirent_dup(de))
		dc->tdc_tree = tmpfs_dir_xlookup_hash(node,
		    de->td_hash);
	return (dc->tdc_current);
}

/*
 * Looks for a directory entry in the directory represented by node.
 * 'cnp' describes the name of the entry to look for.  Note that the .
 * and .. components are not allowed as they do not physically exist
 * within directories.
 *
 * Returns a pointer to the entry when found, otherwise NULL.
 */
struct tmpfs_dirent *
tmpfs_dir_lookup(struct tmpfs_node *node, struct tmpfs_node *f,
    struct componentname *cnp)
{
	struct tmpfs_dir_duphead *duphead;
	struct tmpfs_dirent *de;
	uint32_t hash;

	MPASS(IMPLIES(cnp->cn_namelen == 1, cnp->cn_nameptr[0] != '.'));
	MPASS(IMPLIES(cnp->cn_namelen == 2, !(cnp->cn_nameptr[0] == '.' &&
	    cnp->cn_nameptr[1] == '.')));
	TMPFS_VALIDATE_DIR(node);

	hash = tmpfs_dirent_hash(cnp->cn_nameptr, cnp->cn_namelen);
	de = tmpfs_dir_xlookup_hash(node, hash);
	if (de != NULL && tmpfs_dirent_duphead(de)) {
		duphead = &de->ud.td_duphead;
		LIST_FOREACH(de, duphead, uh.td_dup.entries) {
			if (TMPFS_DIRENT_MATCHES(de, cnp->cn_nameptr,
			    cnp->cn_namelen))
				break;
		}
	} else if (de != NULL) {
		if (!TMPFS_DIRENT_MATCHES(de, cnp->cn_nameptr,
		    cnp->cn_namelen))
			de = NULL;
	}
	if (de != NULL && f != NULL && de->td_node != f)
		de = NULL;

	return (de);
}

/*
 * Attach duplicate-cookie directory entry nde to dnode and insert to dupindex
 * list, allocate new cookie value.
 */
static void
tmpfs_dir_attach_dup(struct tmpfs_node *dnode,
    struct tmpfs_dir_duphead *duphead, struct tmpfs_dirent *nde)
{
	struct tmpfs_dir_duphead *dupindex;
	struct tmpfs_dirent *de, *pde;

	dupindex = &dnode->tn_dir.tn_dupindex;
	de = LIST_FIRST(dupindex);
	if (de == NULL || de->td_cookie < TMPFS_DIRCOOKIE_DUP_MAX) {
		if (de == NULL)
			nde->td_cookie = TMPFS_DIRCOOKIE_DUP_MIN;
		else
			nde->td_cookie = de->td_cookie + 1;
		MPASS(tmpfs_dirent_dup(nde));
		LIST_INSERT_HEAD(dupindex, nde, uh.td_dup.index_entries);
		LIST_INSERT_HEAD(duphead, nde, uh.td_dup.entries);
		return;
	}

	/*
	 * Cookie numbers are near exhaustion. Scan dupindex list for unused
	 * numbers. dupindex list is sorted in descending order. Keep it so
	 * after inserting nde.
	 */
	while (1) {
		pde = de;
		de = LIST_NEXT(de, uh.td_dup.index_entries);
		if (de == NULL && pde->td_cookie != TMPFS_DIRCOOKIE_DUP_MIN) {
			/*
			 * Last element of the index doesn't have minimal cookie
			 * value, use it.
			 */
			nde->td_cookie = TMPFS_DIRCOOKIE_DUP_MIN;
			LIST_INSERT_AFTER(pde, nde, uh.td_dup.index_entries);
			LIST_INSERT_HEAD(duphead, nde, uh.td_dup.entries);
			return;
		} else if (de == NULL) {
			/*
			 * We are so lucky have 2^30 hash duplicates in single
			 * directory :) Return largest possible cookie value.
			 * It should be fine except possible issues with
			 * VOP_READDIR restart.
			 */
			nde->td_cookie = TMPFS_DIRCOOKIE_DUP_MAX;
			LIST_INSERT_HEAD(dupindex, nde,
			    uh.td_dup.index_entries);
			LIST_INSERT_HEAD(duphead, nde, uh.td_dup.entries);
			return;
		}
		if (de->td_cookie + 1 == pde->td_cookie ||
		    de->td_cookie >= TMPFS_DIRCOOKIE_DUP_MAX)
			continue;	/* No hole or invalid cookie. */
		nde->td_cookie = de->td_cookie + 1;
		MPASS(tmpfs_dirent_dup(nde));
		MPASS(pde->td_cookie > nde->td_cookie);
		MPASS(nde->td_cookie > de->td_cookie);
		LIST_INSERT_BEFORE(de, nde, uh.td_dup.index_entries);
		LIST_INSERT_HEAD(duphead, nde, uh.td_dup.entries);
		return;
	};
}

/*
 * Attaches the directory entry de to the directory represented by vp.
 * Note that this does not change the link count of the node pointed by
 * the directory entry, as this is done by tmpfs_alloc_dirent.
 */
void
tmpfs_dir_attach(struct vnode *vp, struct tmpfs_dirent *de)
{
	struct tmpfs_node *dnode;
	struct tmpfs_dirent *xde, *nde;

	ASSERT_VOP_ELOCKED(vp, __func__);
	MPASS(de->td_namelen > 0);
	MPASS(de->td_hash >= TMPFS_DIRCOOKIE_MIN);
	MPASS(de->td_cookie == de->td_hash);

	dnode = VP_TO_TMPFS_DIR(vp);
	dnode->tn_dir.tn_readdir_lastn = 0;
	dnode->tn_dir.tn_readdir_lastp = NULL;

	MPASS(!tmpfs_dirent_dup(de));
	xde = RB_INSERT(tmpfs_dir, &dnode->tn_dir.tn_dirhead, de);
	if (xde != NULL && tmpfs_dirent_duphead(xde))
		tmpfs_dir_attach_dup(dnode, &xde->ud.td_duphead, de);
	else if (xde != NULL) {
		/*
		 * Allocate new duphead. Swap xde with duphead to avoid
		 * adding/removing elements with the same hash.
		 */
		MPASS(!tmpfs_dirent_dup(xde));
		tmpfs_alloc_dirent(VFS_TO_TMPFS(vp->v_mount), NULL, NULL, 0,
		    &nde);
		/* *nde = *xde; XXX gcc 4.2.1 may generate invalid code. */
		memcpy(nde, xde, sizeof(*xde));
		xde->td_cookie |= TMPFS_DIRCOOKIE_DUPHEAD;
		LIST_INIT(&xde->ud.td_duphead);
		xde->td_namelen = 0;
		xde->td_node = NULL;
		tmpfs_dir_attach_dup(dnode, &xde->ud.td_duphead, nde);
		tmpfs_dir_attach_dup(dnode, &xde->ud.td_duphead, de);
	}
	dnode->tn_size += sizeof(struct tmpfs_dirent);
	dnode->tn_status |= TMPFS_NODE_ACCESSED | TMPFS_NODE_CHANGED | \
	    TMPFS_NODE_MODIFIED;
}

/* --------------------------------------------------------------------- */

/*
 * Detaches the directory entry de from the directory represented by vp.
 * Note that this does not change the link count of the node pointed by
 * the directory entry, as this is done by tmpfs_free_dirent.
 */
void
tmpfs_dir_detach(struct vnode *vp, struct tmpfs_dirent *de)
{
	struct tmpfs_mount *tmp;
	struct tmpfs_dir *head;
	struct tmpfs_node *dnode;
	struct tmpfs_dirent *xde;

	ASSERT_VOP_ELOCKED(vp, __func__);

	dnode = VP_TO_TMPFS_DIR(vp);
	head = &dnode->tn_dir.tn_dirhead;
	dnode->tn_dir.tn_readdir_lastn = 0;
	dnode->tn_dir.tn_readdir_lastp = NULL;

	if (tmpfs_dirent_dup(de)) {
		/* Remove duphead if de was last entry. */
		if (LIST_NEXT(de, uh.td_dup.entries) == NULL) {
			xde = tmpfs_dir_xlookup_hash(dnode, de->td_hash);
			MPASS(tmpfs_dirent_duphead(xde));
		} else
			xde = NULL;
		LIST_REMOVE(de, uh.td_dup.entries);
		LIST_REMOVE(de, uh.td_dup.index_entries);
		if (xde != NULL) {
			if (LIST_EMPTY(&xde->ud.td_duphead)) {
				RB_REMOVE(tmpfs_dir, head, xde);
				tmp = VFS_TO_TMPFS(vp->v_mount);
				MPASS(xde->td_node == NULL);
				tmpfs_free_dirent(tmp, xde);
			}
		}
	} else
		RB_REMOVE(tmpfs_dir, head, de);

	dnode->tn_size -= sizeof(struct tmpfs_dirent);
	dnode->tn_status |= TMPFS_NODE_ACCESSED | TMPFS_NODE_CHANGED | \
	    TMPFS_NODE_MODIFIED;
}

void
tmpfs_dir_destroy(struct tmpfs_mount *tmp, struct tmpfs_node *dnode)
{
	struct tmpfs_dirent *de, *dde, *nde;

	RB_FOREACH_SAFE(de, tmpfs_dir, &dnode->tn_dir.tn_dirhead, nde) {
		RB_REMOVE(tmpfs_dir, &dnode->tn_dir.tn_dirhead, de);
		/* Node may already be destroyed. */
		de->td_node = NULL;
		if (tmpfs_dirent_duphead(de)) {
			while ((dde = LIST_FIRST(&de->ud.td_duphead)) != NULL) {
				LIST_REMOVE(dde, uh.td_dup.entries);
				dde->td_node = NULL;
				tmpfs_free_dirent(tmp, dde);
			}
		}
		tmpfs_free_dirent(tmp, de);
	}
}

/* --------------------------------------------------------------------- */

/*
 * Helper function for tmpfs_readdir.  Creates a '.' entry for the given
 * directory and returns it in the uio space.  The function returns 0
 * on success, -1 if there was not enough space in the uio structure to
 * hold the directory entry or an appropriate error code if another
 * error happens.
 */
static int
tmpfs_dir_getdotdent(struct tmpfs_node *node, struct uio *uio)
{
	int error;
	struct dirent dent;

	TMPFS_VALIDATE_DIR(node);
	MPASS(uio->uio_offset == TMPFS_DIRCOOKIE_DOT);

	dent.d_fileno = node->tn_id;
	dent.d_type = DT_DIR;
	dent.d_namlen = 1;
	dent.d_name[0] = '.';
	dent.d_name[1] = '\0';
	dent.d_reclen = GENERIC_DIRSIZ(&dent);

	if (dent.d_reclen > uio->uio_resid)
		error = EJUSTRETURN;
	else
		error = uiomove(&dent, dent.d_reclen, uio);

	node->tn_status |= TMPFS_NODE_ACCESSED;

	return error;
}

/* --------------------------------------------------------------------- */

/*
 * Helper function for tmpfs_readdir.  Creates a '..' entry for the given
 * directory and returns it in the uio space.  The function returns 0
 * on success, -1 if there was not enough space in the uio structure to
 * hold the directory entry or an appropriate error code if another
 * error happens.
 */
static int
tmpfs_dir_getdotdotdent(struct tmpfs_node *node, struct uio *uio)
{
	int error;
	struct dirent dent;

	TMPFS_VALIDATE_DIR(node);
	MPASS(uio->uio_offset == TMPFS_DIRCOOKIE_DOTDOT);

	/*
	 * Return ENOENT if the current node is already removed.
	 */
	TMPFS_ASSERT_LOCKED(node);
	if (node->tn_dir.tn_parent == NULL) {
		return (ENOENT);
	}

	TMPFS_NODE_LOCK(node->tn_dir.tn_parent);
	dent.d_fileno = node->tn_dir.tn_parent->tn_id;
	TMPFS_NODE_UNLOCK(node->tn_dir.tn_parent);

	dent.d_type = DT_DIR;
	dent.d_namlen = 2;
	dent.d_name[0] = '.';
	dent.d_name[1] = '.';
	dent.d_name[2] = '\0';
	dent.d_reclen = GENERIC_DIRSIZ(&dent);

	if (dent.d_reclen > uio->uio_resid)
		error = EJUSTRETURN;
	else
		error = uiomove(&dent, dent.d_reclen, uio);

	node->tn_status |= TMPFS_NODE_ACCESSED;

	return error;
}

/* --------------------------------------------------------------------- */

/*
 * Helper function for tmpfs_readdir.  Returns as much directory entries
 * as can fit in the uio space.  The read starts at uio->uio_offset.
 * The function returns 0 on success, -1 if there was not enough space
 * in the uio structure to hold the directory entry or an appropriate
 * error code if another error happens.
 */
int
tmpfs_dir_getdents(struct tmpfs_node *node, struct uio *uio, int cnt,
    u_long *cookies, int *ncookies)
{
	struct tmpfs_dir_cursor dc;
	struct tmpfs_dirent *de;
	off_t off;
	int error;

	TMPFS_VALIDATE_DIR(node);

	off = 0;
	switch (uio->uio_offset) {
	case TMPFS_DIRCOOKIE_DOT:
		error = tmpfs_dir_getdotdent(node, uio);
		if (error != 0)
			return (error);
		uio->uio_offset = TMPFS_DIRCOOKIE_DOTDOT;
		if (cnt != 0)
			cookies[(*ncookies)++] = off = uio->uio_offset;
	case TMPFS_DIRCOOKIE_DOTDOT:
		error = tmpfs_dir_getdotdotdent(node, uio);
		if (error != 0)
			return (error);
		de = tmpfs_dir_first(node, &dc);
		if (de == NULL)
			uio->uio_offset = TMPFS_DIRCOOKIE_EOF;
		else
			uio->uio_offset = tmpfs_dirent_cookie(de);
		if (cnt != 0)
			cookies[(*ncookies)++] = off = uio->uio_offset;
		if (de == NULL)
			return (0);
		break;
	case TMPFS_DIRCOOKIE_EOF:
		return (0);
	default:
		de = tmpfs_dir_lookup_cookie(node, uio->uio_offset, &dc);
		if (de == NULL)
			return (EINVAL);
		if (cnt != 0)
			off = tmpfs_dirent_cookie(de);
	}

	/* Read as much entries as possible; i.e., until we reach the end of
	 * the directory or we exhaust uio space. */
	do {
		struct dirent d;

		/* Create a dirent structure representing the current
		 * tmpfs_node and fill it. */
		if (de->td_node == NULL) {
			d.d_fileno = 1;
			d.d_type = DT_WHT;
		} else {
			d.d_fileno = de->td_node->tn_id;
			switch (de->td_node->tn_type) {
			case VBLK:
				d.d_type = DT_BLK;
				break;

			case VCHR:
				d.d_type = DT_CHR;
				break;

			case VDIR:
				d.d_type = DT_DIR;
				break;

			case VFIFO:
				d.d_type = DT_FIFO;
				break;

			case VLNK:
				d.d_type = DT_LNK;
				break;

			case VREG:
				d.d_type = DT_REG;
				break;

			case VSOCK:
				d.d_type = DT_SOCK;
				break;

			default:
				panic("tmpfs_dir_getdents: type %p %d",
				    de->td_node, (int)de->td_node->tn_type);
			}
		}
		d.d_namlen = de->td_namelen;
		MPASS(de->td_namelen < sizeof(d.d_name));
		(void)memcpy(d.d_name, de->ud.td_name, de->td_namelen);
		d.d_name[de->td_namelen] = '\0';
		d.d_reclen = GENERIC_DIRSIZ(&d);

		/* Stop reading if the directory entry we are treating is
		 * bigger than the amount of data that can be returned. */
		if (d.d_reclen > uio->uio_resid) {
			error = EJUSTRETURN;
			break;
		}

		/* Copy the new dirent structure into the output buffer and
		 * advance pointers. */
		error = uiomove(&d, d.d_reclen, uio);
		if (error == 0) {
			de = tmpfs_dir_next(node, &dc);
			if (cnt != 0) {
				if (de == NULL)
					off = TMPFS_DIRCOOKIE_EOF;
				else
					off = tmpfs_dirent_cookie(de);
				MPASS(*ncookies < cnt);
				cookies[(*ncookies)++] = off;
			}
		}
	} while (error == 0 && uio->uio_resid > 0 && de != NULL);

	/* Update the offset and cache. */
	if (cnt == 0) {
		if (de == NULL)
			off = TMPFS_DIRCOOKIE_EOF;
		else
			off = tmpfs_dirent_cookie(de);
	}

	uio->uio_offset = off;
	node->tn_dir.tn_readdir_lastn = off;
	node->tn_dir.tn_readdir_lastp = de;

	node->tn_status |= TMPFS_NODE_ACCESSED;
	return error;
}

int
tmpfs_dir_whiteout_add(struct vnode *dvp, struct componentname *cnp)
{
	struct tmpfs_dirent *de;
	int error;

	error = tmpfs_alloc_dirent(VFS_TO_TMPFS(dvp->v_mount), NULL,
	    cnp->cn_nameptr, cnp->cn_namelen, &de);
	if (error != 0)
		return (error);
	tmpfs_dir_attach(dvp, de);
	return (0);
}

void
tmpfs_dir_whiteout_remove(struct vnode *dvp, struct componentname *cnp)
{
	struct tmpfs_dirent *de;

	de = tmpfs_dir_lookup(VP_TO_TMPFS_DIR(dvp), NULL, cnp);
	MPASS(de != NULL && de->td_node == NULL);
	tmpfs_dir_detach(dvp, de);
	tmpfs_free_dirent(VFS_TO_TMPFS(dvp->v_mount), de);
}

/* --------------------------------------------------------------------- */

/*
 * Resizes the aobj associated with the regular file pointed to by 'vp' to the
 * size 'newsize'.  'vp' must point to a vnode that represents a regular file.
 * 'newsize' must be positive.
 *
 * Returns zero on success or an appropriate error code on failure.
 */
int
tmpfs_reg_resize(struct vnode *vp, off_t newsize, boolean_t ignerr)
{
	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;
	vm_object_t uobj;
	vm_page_t m, ma[1];
	vm_pindex_t idx, newpages, oldpages;
	off_t oldsize;
	int base, rv;

	MPASS(vp->v_type == VREG);
	MPASS(newsize >= 0);

	node = VP_TO_TMPFS_NODE(vp);
	uobj = node->tn_reg.tn_aobj;
	tmp = VFS_TO_TMPFS(vp->v_mount);

	/*
	 * Convert the old and new sizes to the number of pages needed to
	 * store them.  It may happen that we do not need to do anything
	 * because the last allocated page can accommodate the change on
	 * its own.
	 */
	oldsize = node->tn_size;
	oldpages = OFF_TO_IDX(oldsize + PAGE_MASK);
	MPASS(oldpages == uobj->size);
	newpages = OFF_TO_IDX(newsize + PAGE_MASK);
	if (newpages > oldpages &&
	    tmpfs_pages_check_avail(tmp, newpages - oldpages) == 0)
		return (ENOSPC);

	VM_OBJECT_LOCK(uobj);
	if (newsize < oldsize) {
		/*
		 * Zero the truncated part of the last page.
		 */
		base = newsize & PAGE_MASK;
		if (base != 0) {
			idx = OFF_TO_IDX(newsize);
retry:
			m = vm_page_lookup(uobj, idx);
			if (m != NULL) {
				if ((m->oflags & VPO_BUSY) != 0 ||
				    m->busy != 0) {
					vm_page_sleep(m, "tmfssz");
					goto retry;
				}
				MPASS(m->valid == VM_PAGE_BITS_ALL);
			} else if (vm_pager_has_page(uobj, idx, NULL, NULL)) {
				m = vm_page_alloc(uobj, idx, VM_ALLOC_NORMAL);
				if (m == NULL) {
					VM_OBJECT_UNLOCK(uobj);
					VM_WAIT;
					VM_OBJECT_LOCK(uobj);
					goto retry;
				} else if (m->valid != VM_PAGE_BITS_ALL) {
					ma[0] = m;
					rv = vm_pager_get_pages(uobj, ma, 1, 0);
					m = vm_page_lookup(uobj, idx);
				} else
					/* A cached page was reactivated. */
					rv = VM_PAGER_OK;
				vm_page_lock(m);
				if (rv == VM_PAGER_OK) {
					vm_page_deactivate(m);
					vm_page_unlock(m);
					vm_page_wakeup(m);
				} else {
					vm_page_free(m);
					vm_page_unlock(m);
					if (ignerr)
						m = NULL;
					else {
						VM_OBJECT_UNLOCK(uobj);
						return (EIO);
					}
				}
			}
			if (m != NULL) {
				pmap_zero_page_area(m, base, PAGE_SIZE - base);
				vm_page_dirty(m);
				vm_pager_page_unswapped(m);
			}
		}

		/*
		 * Release any swap space and free any whole pages.
		 */
		if (newpages < oldpages) {
			swap_pager_freespace(uobj, newpages, oldpages -
			    newpages);
			vm_object_page_remove(uobj, newpages, 0, 0);
		}
	}
	uobj->size = newpages;
	VM_OBJECT_UNLOCK(uobj);

	TMPFS_LOCK(tmp);
	tmp->tm_pages_used += (newpages - oldpages);
	TMPFS_UNLOCK(tmp);

	node->tn_size = newsize;
	vnode_pager_setsize(vp, newsize);
	return (0);
}

/* --------------------------------------------------------------------- */

/*
 * Change flags of the given vnode.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chflags(struct vnode *vp, int flags, struct ucred *cred, struct thread *p)
{
	int error;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	if ((flags & ~(UF_NODUMP | UF_IMMUTABLE | UF_APPEND | UF_OPAQUE |
	    UF_NOUNLINK | SF_ARCHIVED | SF_IMMUTABLE | SF_APPEND |
	    SF_NOUNLINK)) != 0)
		return (EOPNOTSUPP);

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/*
	 * Callers may only modify the file flags on objects they
	 * have VADMIN rights for.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, p)))
		return (error);
	/*
	 * Unprivileged processes are not permitted to unset system
	 * flags, or modify flags if any system flags are set.
	 */
	if (!priv_check_cred(cred, PRIV_VFS_SYSFLAGS, 0)) {
		if (node->tn_flags &
		    (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND)) {
			error = securelevel_gt(cred, 0);
			if (error)
				return (error);
		}
	} else {
		if (node->tn_flags &
		    (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND) ||
		    ((flags ^ node->tn_flags) & SF_SETTABLE))
			return (EPERM);
	}
	node->tn_flags = flags;
	node->tn_status |= TMPFS_NODE_CHANGED;

	MPASS(VOP_ISLOCKED(vp));

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Change access mode on the given vnode.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chmod(struct vnode *vp, mode_t mode, struct ucred *cred, struct thread *p)
{
	int error;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return EPERM;

	/*
	 * To modify the permissions on a file, must possess VADMIN
	 * for that file.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, p)))
		return (error);

	/*
	 * Privileged processes may set the sticky bit on non-directories,
	 * as well as set the setgid bit on a file with a group that the
	 * process is not a member of.
	 */
	if (vp->v_type != VDIR && (mode & S_ISTXT)) {
		if (priv_check_cred(cred, PRIV_VFS_STICKYFILE, 0))
			return (EFTYPE);
	}
	if (!groupmember(node->tn_gid, cred) && (mode & S_ISGID)) {
		error = priv_check_cred(cred, PRIV_VFS_SETGID, 0);
		if (error)
			return (error);
	}


	node->tn_mode &= ~ALLPERMS;
	node->tn_mode |= mode & ALLPERMS;

	node->tn_status |= TMPFS_NODE_CHANGED;

	MPASS(VOP_ISLOCKED(vp));

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Change ownership of the given vnode.  At least one of uid or gid must
 * be different than VNOVAL.  If one is set to that value, the attribute
 * is unchanged.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chown(struct vnode *vp, uid_t uid, gid_t gid, struct ucred *cred,
    struct thread *p)
{
	int error;
	struct tmpfs_node *node;
	uid_t ouid;
	gid_t ogid;

	MPASS(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	/* Assign default values if they are unknown. */
	MPASS(uid != VNOVAL || gid != VNOVAL);
	if (uid == VNOVAL)
		uid = node->tn_uid;
	if (gid == VNOVAL)
		gid = node->tn_gid;
	MPASS(uid != VNOVAL && gid != VNOVAL);

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return EPERM;

	/*
	 * To modify the ownership of a file, must possess VADMIN for that
	 * file.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, p)))
		return (error);

	/*
	 * To change the owner of a file, or change the group of a file to a
	 * group of which we are not a member, the caller must have
	 * privilege.
	 */
	if ((uid != node->tn_uid ||
	    (gid != node->tn_gid && !groupmember(gid, cred))) &&
	    (error = priv_check_cred(cred, PRIV_VFS_CHOWN, 0)))
		return (error);

	ogid = node->tn_gid;
	ouid = node->tn_uid;

	node->tn_uid = uid;
	node->tn_gid = gid;

	node->tn_status |= TMPFS_NODE_CHANGED;

	if ((node->tn_mode & (S_ISUID | S_ISGID)) && (ouid != uid || ogid != gid)) {
		if (priv_check_cred(cred, PRIV_VFS_RETAINSUGID, 0))
			node->tn_mode &= ~(S_ISUID | S_ISGID);
	}

	MPASS(VOP_ISLOCKED(vp));

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Change size of the given vnode.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chsize(struct vnode *vp, u_quad_t size, struct ucred *cred,
    struct thread *p)
{
	int error;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	/* Decide whether this is a valid operation based on the file type. */
	error = 0;
	switch (vp->v_type) {
	case VDIR:
		return EISDIR;

	case VREG:
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		break;

	case VBLK:
		/* FALLTHROUGH */
	case VCHR:
		/* FALLTHROUGH */
	case VFIFO:
		/* Allow modifications of special files even if in the file
		 * system is mounted read-only (we are not modifying the
		 * files themselves, but the objects they represent). */
		return 0;

	default:
		/* Anything else is unsupported. */
		return EOPNOTSUPP;
	}

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return EPERM;

	error = tmpfs_truncate(vp, size);
	/* tmpfs_truncate will raise the NOTE_EXTEND and NOTE_ATTRIB kevents
	 * for us, as will update tn_status; no need to do that here. */

	MPASS(VOP_ISLOCKED(vp));

	return error;
}

/* --------------------------------------------------------------------- */

/*
 * Change access and modification times of the given vnode.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chtimes(struct vnode *vp, struct timespec *atime, struct timespec *mtime,
	struct timespec *birthtime, int vaflags, struct ucred *cred, struct thread *l)
{
	int error;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return EPERM;

	/* Determine if the user have proper privilege to update time. */
	if (vaflags & VA_UTIMES_NULL) {
		error = VOP_ACCESS(vp, VADMIN, cred, l);
		if (error)
			error = VOP_ACCESS(vp, VWRITE, cred, l);
	} else
		error = VOP_ACCESS(vp, VADMIN, cred, l);
	if (error)
		return (error);

	if (atime->tv_sec != VNOVAL && atime->tv_nsec != VNOVAL)
		node->tn_status |= TMPFS_NODE_ACCESSED;

	if (mtime->tv_sec != VNOVAL && mtime->tv_nsec != VNOVAL)
		node->tn_status |= TMPFS_NODE_MODIFIED;

	if (birthtime->tv_nsec != VNOVAL && birthtime->tv_nsec != VNOVAL)
		node->tn_status |= TMPFS_NODE_MODIFIED;

	tmpfs_itimes(vp, atime, mtime);

	if (birthtime->tv_nsec != VNOVAL && birthtime->tv_nsec != VNOVAL)
		node->tn_birthtime = *birthtime;
	MPASS(VOP_ISLOCKED(vp));

	return 0;
}

/* --------------------------------------------------------------------- */
/* Sync timestamps */
void
tmpfs_itimes(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod)
{
	struct tmpfs_node *node;
	struct timespec now;

	node = VP_TO_TMPFS_NODE(vp);

	if ((node->tn_status & (TMPFS_NODE_ACCESSED | TMPFS_NODE_MODIFIED |
	    TMPFS_NODE_CHANGED)) == 0)
		return;

	vfs_timestamp(&now);
	if (node->tn_status & TMPFS_NODE_ACCESSED) {
		if (acc == NULL)
			 acc = &now;
		node->tn_atime = *acc;
	}
	if (node->tn_status & TMPFS_NODE_MODIFIED) {
		if (mod == NULL)
			mod = &now;
		node->tn_mtime = *mod;
	}
	if (node->tn_status & TMPFS_NODE_CHANGED) {
		node->tn_ctime = now;
	}
	node->tn_status &=
	    ~(TMPFS_NODE_ACCESSED | TMPFS_NODE_MODIFIED | TMPFS_NODE_CHANGED);
}

/* --------------------------------------------------------------------- */

void
tmpfs_update(struct vnode *vp)
{

	tmpfs_itimes(vp, NULL, NULL);
}

/* --------------------------------------------------------------------- */

int
tmpfs_truncate(struct vnode *vp, off_t length)
{
	int error;
	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);

	if (length < 0) {
		error = EINVAL;
		goto out;
	}

	if (node->tn_size == length) {
		error = 0;
		goto out;
	}

	if (length > VFS_TO_TMPFS(vp->v_mount)->tm_maxfilesize)
		return (EFBIG);

	error = tmpfs_reg_resize(vp, length, FALSE);
	if (error == 0) {
		node->tn_status |= TMPFS_NODE_CHANGED | TMPFS_NODE_MODIFIED;
	}

out:
	tmpfs_update(vp);

	return error;
}

static __inline int
tmpfs_dirtree_cmp(struct tmpfs_dirent *a, struct tmpfs_dirent *b)
{
	if (a->td_hash > b->td_hash)
		return (1);
	else if (a->td_hash < b->td_hash)
		return (-1);
	return (0);
}

RB_GENERATE_STATIC(tmpfs_dir, tmpfs_dirent, uh.td_entries, tmpfs_dirtree_cmp);
