/*
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <err.h>
#include <fcntl.h>
#include <kvm.h>

#include <defs.h>
#include <readline/readline.h>
#include <readline/tilde.h>
#include <command.h>
#include <exec.h>
#include <frame-unwind.h>
#include <gdbcore.h>
#include <gdbthread.h>
#include <inferior.h>
#include <language.h>
#include <regcache.h>
#include <solib.h>
#include <target.h>

#include "kgdb.h"

static void	kgdb_core_cleanup(void *);

static char *vmcore;
static struct target_ops kgdb_trgt_ops;

kvm_t *kvm;
static char kvm_err[_POSIX2_LINE_MAX];

#define	KERNOFF		(kgdb_kernbase ())
#define	INKERNEL(x)	((x) >= KERNOFF)

static CORE_ADDR
kgdb_kernbase (void)
{
	static CORE_ADDR kernbase;
	struct minimal_symbol *sym;

	if (kernbase == 0) {
		sym = lookup_minimal_symbol ("kernbase", NULL, NULL);
		if (sym == NULL) {
			kernbase = KERNBASE;
		} else {
			kernbase = SYMBOL_VALUE_ADDRESS (sym);
		}
	}
	return kernbase;
}

static void
kgdb_trgt_open(char *filename, int from_tty)
{
	struct cleanup *old_chain;
	struct thread_info *ti;
	struct kthr *kt;
	kvm_t *nkvm;
	char *temp;
	int ontop;

	target_preopen (from_tty);
	if (!filename)
		error ("No vmcore file specified.");
	if (!exec_bfd)
		error ("Can't open a vmcore without a kernel");

	filename = tilde_expand (filename);
	if (filename[0] != '/') {
		temp = concat (current_directory, "/", filename, NULL);
		xfree(filename);
		filename = temp;
	}

	old_chain = make_cleanup (xfree, filename);

	nkvm = kvm_openfiles(bfd_get_filename(exec_bfd), filename, NULL,
	    write_files ? O_RDWR : O_RDONLY, kvm_err);
	if (nkvm == NULL)
		error ("Failed to open vmcore: %s", kvm_err);

	/* Don't free the filename now and close any previous vmcore. */
	discard_cleanups(old_chain);
	unpush_target(&kgdb_trgt_ops);

	kvm = nkvm;
	vmcore = filename;
	old_chain = make_cleanup(kgdb_core_cleanup, NULL);

	ontop = !push_target (&kgdb_trgt_ops);
	discard_cleanups (old_chain);

	kgdb_dmesg();

	init_thread_list();
	kt = kgdb_thr_init();
	while (kt != NULL) {
		ti = add_thread(ptid_build(kt->pid, 0, kt->tid));
		kt = kgdb_thr_next(kt);
	}
	if (curkthr != 0)
		inferior_ptid = ptid_build(curkthr->pid, 0, curkthr->tid);

	if (ontop) {
		/* XXX: fetch registers? */
		kld_init();
		flush_cached_frames();
		select_frame (get_current_frame());
		print_stack_frame(get_selected_frame(),
		    frame_relative_level(get_selected_frame()), 1);
	} else
		warning(
	"you won't be able to access this vmcore until you terminate\n\
your %s; do ``info files''", target_longname);
}

static void
kgdb_trgt_close(int quitting)
{

	if (kvm != NULL) {		
		inferior_ptid = null_ptid;
		CLEAR_SOLIB();
		if (kvm_close(kvm) != 0)
			warning("cannot close \"%s\": %s", vmcore,
			    kvm_geterr(kvm));
		kvm = NULL;
		xfree(vmcore);
		vmcore = NULL;
		if (kgdb_trgt_ops.to_sections) {
			xfree(kgdb_trgt_ops.to_sections);
			kgdb_trgt_ops.to_sections = NULL;
			kgdb_trgt_ops.to_sections_end = NULL;
		}
	}
}

static void
kgdb_core_cleanup(void *arg)
{

	kgdb_trgt_close(0);
}

static void
kgdb_trgt_detach(char *args, int from_tty)
{

	if (args)
		error ("Too many arguments");
	unpush_target(&kgdb_trgt_ops);
	reinit_frame_cache();
	if (from_tty)
		printf_filtered("No vmcore file now.\n");
}

static char *
kgdb_trgt_extra_thread_info(struct thread_info *ti)
{
	static char buf[64];
	char *p, *s;

	p = buf + snprintf(buf, sizeof(buf), "PID=%d", ptid_get_pid(ti->ptid));
	s = kgdb_thr_extra_thread_info(ptid_get_tid(ti->ptid));
	if (s != NULL)
		snprintf(p, sizeof(buf) - (p - buf), ": %s", s);
	return (buf);
}

static void
kgdb_trgt_files_info(struct target_ops *target)
{

	printf_filtered ("\t`%s', ", vmcore);
	wrap_here ("        ");
	printf_filtered ("file type %s.\n", "FreeBSD kernel vmcore");
}

static void
kgdb_trgt_find_new_threads(void)
{
	struct target_ops *tb;

	if (kvm != NULL)
		return;

	tb = find_target_beneath(&kgdb_trgt_ops);
	if (tb->to_find_new_threads != NULL)
		tb->to_find_new_threads();
}

static char *
kgdb_trgt_pid_to_str(ptid_t ptid)
{
	static char buf[33];

	snprintf(buf, sizeof(buf), "Thread %ld", ptid_get_tid(ptid));
	return (buf);
}

static int
kgdb_trgt_thread_alive(ptid_t ptid)
{
	return (kgdb_thr_lookup_tid(ptid_get_tid(ptid)) != NULL);
}

static int
kgdb_trgt_xfer_memory(CORE_ADDR memaddr, char *myaddr, int len, int write,
    struct mem_attrib *attrib, struct target_ops *target)
{
	struct target_ops *tb;

	if (kvm != NULL) {
		if (len == 0)
			return (0);
		if (!write)
			return (kvm_read(kvm, memaddr, myaddr, len));
		else
			return (kvm_write(kvm, memaddr, myaddr, len));
	}
	tb = find_target_beneath(target);
	return (tb->to_xfer_memory(memaddr, myaddr, len, write, attrib, tb));
}

static int
kgdb_trgt_ignore_breakpoints(CORE_ADDR addr, char *contents)
{

	return 0;
}

static void
kgdb_switch_to_thread(struct kthr *thr)
{
	if (thr->tid == ptid_get_tid(inferior_ptid))
		return;

	inferior_ptid = ptid_build(thr->pid, 0, thr->tid);
	flush_cached_frames ();
	registers_changed ();
	stop_pc = read_pc ();
	select_frame (get_current_frame ());
}

static void
kgdb_set_proc_cmd (char *arg, int from_tty)
{
	CORE_ADDR addr;
	struct kthr *thr;

	if (!arg)
		error_no_arg ("proc address for the new context");

	if (kvm == NULL)
		error ("no kernel core file");

	addr = (CORE_ADDR) parse_and_eval_address (arg);

	if (!INKERNEL (addr)) {
		thr = kgdb_thr_lookup_pid((int)addr);
		if (thr == NULL)
			error ("invalid pid");
	} else {
		thr = kgdb_thr_lookup_paddr(addr);
		if (thr == NULL)
			error("invalid proc address");
	}
	kgdb_switch_to_thread(thr);
}

static void
kgdb_set_tid_cmd (char *arg, int from_tty)
{
	CORE_ADDR addr;
	struct kthr *thr;

	if (!arg)
		error_no_arg ("TID or thread address for the new context");

	if (kvm == NULL)
		error ("no kernel core file");

	addr = (CORE_ADDR) parse_and_eval_address (arg);

	if (!INKERNEL (addr)) {
		thr = kgdb_thr_lookup_tid((int)addr);
		if (thr == NULL)
			error ("invalid TID");
	} else {
		thr = kgdb_thr_lookup_taddr(addr);
		if (thr == NULL)
			error("invalid thread address");
	}
	kgdb_switch_to_thread(thr);
}

int fbsdcoreops_suppress_target = 1;

void
initialize_kgdb_target(void)
{

	kgdb_trgt_ops.to_magic = OPS_MAGIC;
	kgdb_trgt_ops.to_shortname = "kernel";
	kgdb_trgt_ops.to_longname = "kernel core dump file";
	kgdb_trgt_ops.to_doc = 
    "Use a vmcore file as a target.  Specify the filename of the vmcore file.";
	kgdb_trgt_ops.to_stratum = core_stratum;
	kgdb_trgt_ops.to_has_memory = 1;
	kgdb_trgt_ops.to_has_registers = 1;
	kgdb_trgt_ops.to_has_stack = 1;

	kgdb_trgt_ops.to_open = kgdb_trgt_open;
	kgdb_trgt_ops.to_close = kgdb_trgt_close;
	kgdb_trgt_ops.to_attach = find_default_attach;
	kgdb_trgt_ops.to_detach = kgdb_trgt_detach;
	kgdb_trgt_ops.to_extra_thread_info = kgdb_trgt_extra_thread_info;
	kgdb_trgt_ops.to_fetch_registers = kgdb_trgt_fetch_registers;
	kgdb_trgt_ops.to_files_info = kgdb_trgt_files_info;
	kgdb_trgt_ops.to_find_new_threads = kgdb_trgt_find_new_threads;
	kgdb_trgt_ops.to_pid_to_str = kgdb_trgt_pid_to_str;
	kgdb_trgt_ops.to_store_registers = kgdb_trgt_store_registers;
	kgdb_trgt_ops.to_thread_alive = kgdb_trgt_thread_alive;
	kgdb_trgt_ops.to_xfer_memory = kgdb_trgt_xfer_memory;
	kgdb_trgt_ops.to_insert_breakpoint = kgdb_trgt_ignore_breakpoints;
	kgdb_trgt_ops.to_remove_breakpoint = kgdb_trgt_ignore_breakpoints;

	add_target(&kgdb_trgt_ops);

	add_com ("proc", class_obscure, kgdb_set_proc_cmd,
	   "Set current process context");
	add_com ("tid", class_obscure, kgdb_set_tid_cmd,
	   "Set current thread context");
}
