/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <libgeom.h>
#include <geom/raid3/g_raid3.h>
#include <core/geom.h>
#include <misc/subr.h>


uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_RAID3_VERSION;

static void raid3_main(struct gctl_req *req, unsigned f);
static void raid3_clear(struct gctl_req *req);
static void raid3_dump(struct gctl_req *req);
static void raid3_label(struct gctl_req *req);

struct g_command class_commands[] = {
	{ "clear", G_FLAG_VERBOSE, raid3_main, G_NULL_OPTS },
	{ "configure", G_FLAG_VERBOSE, NULL,
	    {
		{ 'a', "autosync", NULL, G_TYPE_NONE },
		{ 'd', "dynamic", NULL, G_TYPE_NONE },
		{ 'h', "hardcode", NULL, G_TYPE_NONE },
		{ 'n', "noautosync", NULL, G_TYPE_NONE },
		{ 'r', "round_robin", NULL, G_TYPE_NONE },
		{ 'R', "noround_robin", NULL, G_TYPE_NONE },
		{ 'w', "verify", NULL, G_TYPE_NONE },
		{ 'W', "noverify", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    }
	},
	{ "dump", 0, raid3_main, G_NULL_OPTS },
	{ "insert", G_FLAG_VERBOSE, NULL,
	    {
		{ 'h', "hardcode", NULL, G_TYPE_NONE },
		{ 'n', "number", NULL, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    }
	},
	{ "label", G_FLAG_VERBOSE, raid3_main,
	    {
		{ 'h', "hardcode", NULL, G_TYPE_NONE },
		{ 'n', "noautosync", NULL, G_TYPE_NONE },
		{ 'r', "round_robin", NULL, G_TYPE_NONE },
		{ 'w', "verify", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    }
	},
	{ "rebuild", G_FLAG_VERBOSE, NULL, G_NULL_OPTS },
	{ "remove", G_FLAG_VERBOSE, NULL,
	    {
		{ 'n', "number", NULL, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    }
	},
	{ "stop", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    }
	},
	G_CMD_SENTINEL
};

static int verbose = 0;

void usage(const char *);
void
usage(const char *comm)
{
	fprintf(stderr,
	    "usage: %s label [-hnrvw] name prov prov prov [prov [...]]\n"
	    "       %s clear [-v] prov [prov [...]]\n"
	    "       %s dump prov [prov [...]]\n"
	    "       %s configure [-adhnrRvwW] name\n"
	    "       %s rebuild [-v] name prov\n"
	    "       %s insert [-hv] <-n number> name prov\n"
	    "       %s remove [-v] <-n number> name\n"
	    "       %s stop [-fv] name [...]\n",
	    comm, comm, comm, comm, comm, comm, comm, comm);
	exit(EXIT_FAILURE);
}

static void
raid3_main(struct gctl_req *req, unsigned flags)
{
	const char *name;

	if ((flags & G_FLAG_VERBOSE) != 0)
		verbose = 1;

	name = gctl_get_asciiparam(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
	if (strcmp(name, "label") == 0)
		raid3_label(req);
	else if (strcmp(name, "clear") == 0)
		raid3_clear(req);
	else if (strcmp(name, "dump") == 0)
		raid3_dump(req);
	else
		gctl_error(req, "Unknown command: %s.", name);
}

static void
raid3_label(struct gctl_req *req)
{
	struct g_raid3_metadata md;
	u_char sector[512];
	const char *str;
	char param[16];
	int *hardcode, *nargs, *noautosync, *round_robin, *verify;
	int error, i;
	unsigned sectorsize, ssize;
	off_t mediasize, msize;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 4) {
		gctl_error(req, "Too few arguments.");
		return;
	}
#ifndef BITCOUNT
#define	BITCOUNT(x)	(((BX_(x) + (BX_(x) >> 4)) & 0x0F0F0F0F) % 255)
#define	BX_(x)		((x) - (((x) >> 1) & 0x77777777) -		\
			 (((x) >> 2) & 0x33333333) - (((x) >> 3) & 0x11111111))
#endif
	if (BITCOUNT(*nargs - 2) != 1) {
		gctl_error(req, "Invalid number of components.");
		return;
	}

	strlcpy(md.md_magic, G_RAID3_MAGIC, sizeof(md.md_magic));
	md.md_version = G_RAID3_VERSION;
	str = gctl_get_asciiparam(req, "arg0");
	if (str == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	strlcpy(md.md_name, str, sizeof(md.md_name));
	md.md_all = *nargs - 1;
	md.md_mflags = 0;
	md.md_dflags = 0;
	md.md_syncid = 1;
	md.md_sync_offset = 0;
	noautosync = gctl_get_paraml(req, "noautosync", sizeof(*noautosync));
	if (noautosync == NULL) {
		gctl_error(req, "No '%s' argument.", "noautosync");
		return;
	}
	if (*noautosync)
		md.md_mflags |= G_RAID3_DEVICE_FLAG_NOAUTOSYNC;
	round_robin = gctl_get_paraml(req, "round_robin", sizeof(*round_robin));
	if (round_robin == NULL) {
		gctl_error(req, "No '%s' argument.", "round_robin");
		return;
	}
	if (*round_robin)
		md.md_mflags |= G_RAID3_DEVICE_FLAG_ROUND_ROBIN;
	verify = gctl_get_paraml(req, "verify", sizeof(*verify));
	if (verify == NULL) {
		gctl_error(req, "No '%s' argument.", "verify");
		return;
	}
	if (*verify)
		md.md_mflags |= G_RAID3_DEVICE_FLAG_VERIFY;
	if (*round_robin && *verify) {
		gctl_error(req, "Both '%c' and '%c' options given.", 'r', 'w');
		return;
	}
	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No '%s' argument.", "hardcode");
		return;
	}

	/*
	 * Calculate sectorsize by finding least common multiple from
	 * sectorsizes of every disk and find the smallest mediasize.
	 */
	mediasize = 0;
	sectorsize = 0;
	for (i = 1; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		str = gctl_get_asciiparam(req, param);

		msize = g_get_mediasize(str);
		ssize = g_get_sectorsize(str);
		if (msize == 0 || ssize == 0) {
			gctl_error(req, "Can't get informations about %s: %s.",
			    str, strerror(errno));
			return;
		}
		msize -= ssize;
		if (mediasize == 0 || (mediasize > 0 && msize < mediasize))
			mediasize = msize;
		if (sectorsize == 0)
			sectorsize = ssize;
		else
			sectorsize = g_lcm(sectorsize, ssize);
	}
	md.md_mediasize = mediasize * (*nargs - 2);
	md.md_sectorsize = sectorsize * (*nargs - 2);

	/*
	 * Clear last sector first, to spoil all components if device exists.
	 */
	for (i = 1; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		str = gctl_get_asciiparam(req, param);

		error = g_metadata_clear(str, NULL);
		if (error != 0) {
			gctl_error(req, "Can't store metadata on %s: %s.", str,
			    strerror(error));
			return;
		}
	}

	/*
	 * Ok, store metadata (use disk number as priority).
	 */
	for (i = 1; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		str = gctl_get_asciiparam(req, param);

		msize = g_get_mediasize(str) - g_get_sectorsize(str);
		if (mediasize < msize) {
			fprintf(stderr,
			    "warning: %s: only %jd bytes from %jd bytes used.\n",
			    str, (intmax_t)mediasize, (intmax_t)msize);
		}

		md.md_no = i - 1;
		if (!*hardcode)
			bzero(md.md_provider, sizeof(md.md_provider));
		else {
			if (strncmp(str, _PATH_DEV, strlen(_PATH_DEV)) == 0)
				str += strlen(_PATH_DEV);
			strlcpy(md.md_provider, str, sizeof(md.md_provider));
		}
		raid3_metadata_encode(&md, sector);
		error = g_metadata_store(str, sector, sizeof(sector));
		if (error != 0) {
			fprintf(stderr, "Can't store metadata on %s: %s.\n",
			    str, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata value stored on %s.\n", str);
	}
}

static void
raid3_clear(struct gctl_req *req)
{
	const char *name;
	char param[16];
	int *nargs, error, i;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		error = g_metadata_clear(name, G_RAID3_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't clear metadata on %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata cleared on %s.\n", name); 
	}
}

static void
raid3_dump(struct gctl_req *req)
{
	struct g_raid3_metadata md, tmpmd;
	const char *name;
	char param[16];
	int *nargs, error, i;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		error = g_metadata_read(name, (u_char *)&tmpmd, sizeof(tmpmd),
		    G_RAID3_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't read metadata from %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (raid3_metadata_decode((u_char *)&tmpmd, &md) != 0) {
			fprintf(stderr, "MD5 hash mismatch for %s, skipping.\n",
			    name);
			gctl_error(req, "Not fully done.");
			continue;
		}
		printf("Metadata on %s:\n", name);
		raid3_metadata_dump(&md);
		printf("\n");
	}
}
