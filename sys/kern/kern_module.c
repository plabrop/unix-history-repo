/*-
 * Copyright (c) 1997 Doug Rabson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

static MALLOC_DEFINE(M_MODULE, "module", "module data structures");

typedef TAILQ_HEAD(, module) modulelist_t;
struct module {
    TAILQ_ENTRY(module)	link;		/* chain together all modules */
    TAILQ_ENTRY(module)	flink;		/* all modules in a file */
    struct linker_file*	file;		/* file which contains this module */
    int			refs;		/* reference count */
    int			id;		/* unique id number */
    char		*name;		/* module name */
    modeventhand_t	handler;	/* event handler */
    void		*arg;		/* argument for handler */
    modspecific_t	data;		/* module specific data */
};

#define MOD_EVENT(mod, type) (mod)->handler((mod), (type), (mod)->arg)

static modulelist_t modules;
static int nextid = 1;

static void module_shutdown(void*, int);

static int
modevent_nop(module_t mod, int what, void* arg)
{
	return 0;
}


static void
module_init(void* arg)
{
    TAILQ_INIT(&modules);
    EVENTHANDLER_REGISTER(shutdown_post_sync, module_shutdown, NULL,
			  SHUTDOWN_PRI_DEFAULT);
}

SYSINIT(module, SI_SUB_KLD, SI_ORDER_FIRST, module_init, 0);

static void
module_shutdown(void* arg1, int arg2)
{
    module_t mod;

    TAILQ_FOREACH(mod, &modules, link)
	MOD_EVENT(mod, MOD_SHUTDOWN);
}

void
module_register_init(const void *arg)
{
    const moduledata_t* data = (const moduledata_t*) arg;
    int error;
    module_t mod;

    mod = module_lookupbyname(data->name);
    if (mod == NULL)
	panic("module_register_init: module named %s not found\n", data->name);
    error = MOD_EVENT(mod, MOD_LOAD);
    if (error) {
	MOD_EVENT(mod, MOD_UNLOAD);
	module_release(mod);
	printf("module_register_init: MOD_LOAD (%s, %lx, %p) error %d\n",
	       data->name, (u_long)(uintfptr_t)data->evhand, data->priv, error);
    }
}

int
module_register(const moduledata_t *data, linker_file_t container)
{
    size_t namelen;
    module_t newmod;

    newmod = module_lookupbyname(data->name);
    if (newmod != NULL) {
	printf("module_register: module %s already exists!\n", data->name);
	return EEXIST;
    }
    namelen = strlen(data->name) + 1;
    newmod = (module_t) malloc(sizeof(struct module) + namelen,
			       M_MODULE, M_WAITOK);
    if (newmod == 0)
	return ENOMEM;

    newmod->refs = 1;
    newmod->id = nextid++;
    newmod->name = (char *) (newmod + 1);
    strcpy(newmod->name, data->name);
    newmod->handler = data->evhand ? data->evhand : modevent_nop;
    newmod->arg = data->priv;
    bzero(&newmod->data, sizeof(newmod->data));
    TAILQ_INSERT_TAIL(&modules, newmod, link);

    if (container)
	TAILQ_INSERT_TAIL(&container->modules, newmod, flink);
    newmod->file = container;

    return 0;
}

void
module_reference(module_t mod)
{
    MOD_DPF(REFS, ("module_reference: before, refs=%d\n", mod->refs));

    mod->refs++;
}

void
module_release(module_t mod)
{
    if (mod->refs <= 0)
	panic("module_release: bad reference count");

    MOD_DPF(REFS, ("module_release: before, refs=%d\n", mod->refs));

    mod->refs--;
    if (mod->refs == 0) {
	TAILQ_REMOVE(&modules, mod, link);
	if (mod->file) {
	    TAILQ_REMOVE(&mod->file->modules, mod, flink);
	}
	free(mod, M_MODULE);
    }
}

module_t
module_lookupbyname(const char* name)
{
    module_t mod;

    TAILQ_FOREACH(mod, &modules, link) {
	if (!strcmp(mod->name, name))
	    return mod;
    }

    return 0;
}

module_t
module_lookupbyid(int modid)
{
    module_t mod;

    TAILQ_FOREACH(mod, &modules, link) {
	if (mod->id == modid)
	    return mod;
    }

    return 0;
}

int
module_unload(module_t mod)
{
    return MOD_EVENT(mod, MOD_UNLOAD);
}

int
module_getid(module_t mod)
{
    return mod->id;
}

module_t
module_getfnext(module_t mod)
{
    return TAILQ_NEXT(mod, flink);
}

void
module_setspecific(module_t mod, modspecific_t *datap)
{
    mod->data = *datap;
}

/*
 * Syscalls.
 */
/*
 * MPSAFE
 */
int
modnext(struct thread *td, struct modnext_args *uap)
{
    module_t mod;
    int error = 0;

    mtx_lock(&Giant);

    td->td_retval[0] = -1;
    if (SCARG(uap, modid) == 0) {
	mod = TAILQ_FIRST(&modules);
	if (mod)
	    td->td_retval[0] = mod->id;
	else
	    error = ENOENT;
	goto done2;
    }

    mod = module_lookupbyid(SCARG(uap, modid));
    if (mod == NULL) {
	error = ENOENT;
	goto done2;
    }

    if (TAILQ_NEXT(mod, link))
	td->td_retval[0] = TAILQ_NEXT(mod, link)->id;
    else
	td->td_retval[0] = 0;
done2:
    mtx_unlock(&Giant);
    return (error);
}

/*
 * MPSAFE
 */
int
modfnext(struct thread *td, struct modfnext_args *uap)
{
    module_t mod;
    int error;

    td->td_retval[0] = -1;

    mtx_lock(&Giant);

    mod = module_lookupbyid(SCARG(uap, modid));
    if (mod == NULL) {
	error = ENOENT;
    } else {
	error = 0;
	if (TAILQ_NEXT(mod, flink))
	    td->td_retval[0] = TAILQ_NEXT(mod, flink)->id;
	else
	    td->td_retval[0] = 0;
    }
    mtx_unlock(&Giant);
    return (error);
}

struct module_stat_v1 {
    int		version;	/* set to sizeof(struct module_stat) */
    char	name[MAXMODNAME];
    int		refs;
    int		id;
};

/*
 * MPSAFE
 */
int
modstat(struct thread *td, struct modstat_args *uap)
{
    module_t mod;
    int error = 0;
    int namelen;
    int version;
    struct module_stat* stat;

    mtx_lock(&Giant);

    mod = module_lookupbyid(SCARG(uap, modid));
    if (mod == NULL) {
	error = ENOENT;
	goto out;
    }

    stat = SCARG(uap, stat);

    /*
     * Check the version of the user's structure.
     */
    if ((error = copyin(&stat->version, &version, sizeof(version))) != 0)
	goto out;
    if (version != sizeof(struct module_stat_v1)
	&& version != sizeof(struct module_stat)) {
	error = EINVAL;
	goto out;
    }

    namelen = strlen(mod->name) + 1;
    if (namelen > MAXMODNAME)
	namelen = MAXMODNAME;
    if ((error = copyout(mod->name, &stat->name[0], namelen)) != 0)
	goto out;

    if ((error = copyout(&mod->refs, &stat->refs, sizeof(int))) != 0)
	goto out;
    if ((error = copyout(&mod->id, &stat->id, sizeof(int))) != 0)
	goto out;

    /*
     * >v1 stat includes module data.
     */
    if (version == sizeof(struct module_stat)) {
	if ((error = copyout(&mod->data, &stat->data, sizeof(mod->data))) != 0)
	    goto out;
    }

    td->td_retval[0] = 0;

out:
    mtx_unlock(&Giant);
    return error;
}

/*
 * MPSAFE
 */
int
modfind(struct thread *td, struct modfind_args *uap)
{
    int error = 0;
    char name[MAXMODNAME];
    module_t mod;

    if ((error = copyinstr(SCARG(uap, name), name, sizeof name, 0)) != 0)
	goto out;

    mtx_lock(&Giant);
    mod = module_lookupbyname(name);
    if (mod == NULL)
	error = ENOENT;
    else
	td->td_retval[0] = mod->id;
    mtx_unlock(&Giant);
out:
    return error;
}
