/*
 * Copyright (C) 2004-2009, 2012-2014  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */

#ifdef WIN32
/*
 * Silence compiler warnings about using strcpy and friends.
 */
#define _CRT_SECURE_NO_DEPRECATE 1
/*
 * We use snprintf which was defined late in Windows even it is in C99.
 */
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WIN32
#include "gen-win32.h"
#else
#include "gen-unix.h"
#endif

#define INSIST(cond) \
	if (!(cond)) { \
		fprintf(stderr, "%s:%d: INSIST(%s)\n", \
			 __FILE__, __LINE__, #cond); \
		abort(); \
	}

#define FROMTEXTARGS "rdclass, type, lexer, origin, options, target, callbacks"
#define FROMTEXTCLASS "rdclass"
#define FROMTEXTTYPE "type"
#define FROMTEXTDEF "result = DNS_R_UNKNOWN"

#define TOTEXTARGS "rdata, tctx, target"
#define TOTEXTCLASS "rdata->rdclass"
#define TOTEXTTYPE "rdata->type"
#define TOTEXTDEF "use_default = ISC_TRUE"

#define FROMWIREARGS "rdclass, type, source, dctx, options, target"
#define FROMWIRECLASS "rdclass"
#define FROMWIRETYPE "type"
#define FROMWIREDEF "use_default = ISC_TRUE"

#define TOWIREARGS "rdata, cctx, target"
#define TOWIRECLASS "rdata->rdclass"
#define TOWIRETYPE "rdata->type"
#define TOWIREDEF "use_default = ISC_TRUE"

#define FROMSTRUCTARGS "rdclass, type, source, target"
#define FROMSTRUCTCLASS "rdclass"
#define FROMSTRUCTTYPE "type"
#define FROMSTRUCTDEF "use_default = ISC_TRUE"

#define TOSTRUCTARGS "rdata, target, mctx"
#define TOSTRUCTCLASS "rdata->rdclass"
#define TOSTRUCTTYPE "rdata->type"
#define TOSTRUCTDEF "use_default = ISC_TRUE"

#define FREESTRUCTARGS "source"
#define FREESTRUCTCLASS "common->rdclass"
#define FREESTRUCTTYPE "common->rdtype"
#define FREESTRUCTDEF NULL

#define COMPAREARGS "rdata1, rdata2"
#define COMPARECLASS "rdata1->rdclass"
#define COMPARETYPE "rdata1->type"
#define COMPAREDEF "use_default = ISC_TRUE"

#define ADDITIONALDATAARGS "rdata, add, arg"
#define ADDITIONALDATACLASS "rdata->rdclass"
#define ADDITIONALDATATYPE "rdata->type"
#define ADDITIONALDATADEF "use_default = ISC_TRUE"

#define DIGESTARGS "rdata, digest, arg"
#define DIGESTCLASS "rdata->rdclass"
#define DIGESTTYPE "rdata->type"
#define DIGESTDEF "use_default = ISC_TRUE"

#define CHECKOWNERARGS "name, rdclass, type, wildcard"
#define CHECKOWNERCLASS "rdclass"
#define CHECKOWNERTYPE "type"
#define CHECKOWNERDEF "result = ISC_TRUE"

#define CHECKNAMESARGS "rdata, owner, bad"
#define CHECKNAMESCLASS "rdata->rdclass"
#define CHECKNAMESTYPE "rdata->type"
#define CHECKNAMESDEF "result = ISC_TRUE"

static const char copyright[] =
"/*\n"
" * Copyright (C) 2004%s Internet Systems Consortium, Inc. (\"ISC\")\n"
" * Copyright (C) 1998-2003 Internet Software Consortium.\n"
" *\n"
" * Permission to use, copy, modify, and distribute this software for any\n"
" * purpose with or without fee is hereby granted, provided that the above\n"
" * copyright notice and this permission notice appear in all copies.\n"
" *\n"
" * THE SOFTWARE IS PROVIDED \"AS IS\" AND ISC DISCLAIMS ALL WARRANTIES WITH\n"
" * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY\n"
" * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,\n"
" * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM\n"
" * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE\n"
" * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR\n"
" * PERFORMANCE OF THIS SOFTWARE.\n"
" */\n"
"\n"
"/***************\n"
" ***************\n"
" ***************   THIS FILE IS AUTOMATICALLY GENERATED BY gen.c.\n"
" ***************   DO NOT EDIT!\n"
" ***************\n"
" ***************/\n"
"\n"
"/*! \\file */\n"
"\n";

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

#define TYPENAMES 256
#define TYPECLASSLEN 20		/* DNS mnemonic size. Must be less than 100. */
#define TYPECLASSBUF (TYPECLASSLEN + 1)
#define TYPECLASSFMT "%" STR(TYPECLASSLEN) "[-0-9a-z]_%d"
#define ATTRIBUTESIZE 256
#define DIRNAMESIZE 256

static struct cc {
	struct cc *next;
	int rdclass;
	char classname[TYPECLASSBUF];
} *classes;

static struct tt {
	struct tt *next;
	int rdclass;
	int type;
	char classname[TYPECLASSBUF];
	char typename[TYPECLASSBUF];
	char dirname[DIRNAMESIZE];	/* XXX Should be max path length */
} *types;

static struct ttnam {
	char typename[TYPECLASSBUF];
	char macroname[TYPECLASSBUF];
	char attr[ATTRIBUTESIZE];
	unsigned int sorted;
	int type;
} typenames[TYPENAMES];

static int maxtype = -1;

static char *
upper(char *);
static char *
funname(const char *, char *);
static void
doswitch(const char *, const char *, const char *, const char *,
	 const char *, const char *);
static void
add(int, const char *, int, const char *, const char *);
static void
sd(int, const char *, const char *, char);
static void
insert_into_typenames(int, const char *, const char *);

/*%
 * If you use more than 10 of these in, say, a printf(), you'll have problems.
 */
static char *
upper(char *s) {
	static int buf_to_use = 0;
	static char buf[10][256];
	char *b;
	int c;

	buf_to_use++;
	if (buf_to_use > 9)
		buf_to_use = 0;

	b = buf[buf_to_use];
	memset(b, 0, 256);

	while ((c = (*s++) & 0xff))
		*b++ = islower(c) ? toupper(c) : c;
	*b = '\0';
	return (buf[buf_to_use]);
}

static char *
funname(const char *s, char *buf) {
	char *b = buf;
	char c;

	INSIST(strlen(s) < TYPECLASSBUF);
	while ((c = *s++)) {
		*b++ = (c == '-') ? '_' : c;
	}
	*b = '\0';
	return (buf);
}

static void
doswitch(const char *name, const char *function, const char *args,
	 const char *tsw, const char *csw, const char *res)
{
	struct tt *tt;
	int first = 1;
	int lasttype = 0;
	int subswitch = 0;
	char buf1[TYPECLASSBUF], buf2[TYPECLASSBUF];
	const char *result = " result =";

	if (res == NULL)
		result = "";

	for (tt = types; tt != NULL; tt = tt->next) {
		if (first) {
			fprintf(stdout, "\n#define %s \\\n", name);
			fprintf(stdout, "\tswitch (%s) { \\\n" /*}*/, tsw);
			first = 0;
		}
		if (tt->type != lasttype && subswitch) {
			if (res == NULL)
				fprintf(stdout, "\t\tdefault: break; \\\n");
			else
				fprintf(stdout,
					"\t\tdefault: %s; break; \\\n", res);
			fputs(/*{*/ "\t\t} \\\n", stdout);
			fputs("\t\tbreak; \\\n", stdout);
			subswitch = 0;
		}
		if (tt->rdclass && tt->type != lasttype) {
			fprintf(stdout, "\tcase %d: switch (%s) { \\\n" /*}*/,
				tt->type, csw);
			subswitch = 1;
		}
		if (tt->rdclass == 0)
			fprintf(stdout,
				"\tcase %d:%s %s_%s(%s); break;",
				tt->type, result, function,
				funname(tt->typename, buf1), args);
		else
			fprintf(stdout,
				"\t\tcase %d:%s %s_%s_%s(%s); break;",
				tt->rdclass, result, function,
				funname(tt->classname, buf1),
				funname(tt->typename, buf2), args);
		fputs(" \\\n", stdout);
		lasttype = tt->type;
	}
	if (subswitch) {
		if (res == NULL)
			fprintf(stdout, "\t\tdefault: break; \\\n");
		else
			fprintf(stdout, "\t\tdefault: %s; break; \\\n", res);
		fputs(/*{*/ "\t\t} \\\n", stdout);
		fputs("\t\tbreak; \\\n", stdout);
	}
	if (first) {
		if (res == NULL)
			fprintf(stdout, "\n#define %s\n", name);
		else
			fprintf(stdout, "\n#define %s %s;\n", name, res);
	} else {
		if (res == NULL)
			fprintf(stdout, "\tdefault: break; \\\n");
		else
			fprintf(stdout, "\tdefault: %s; break; \\\n", res);
		fputs(/*{*/ "\t}\n", stdout);
	}
}

static struct ttnam *
find_typename(int type) {
	int i;

	for (i = 0; i < TYPENAMES; i++) {
		if (typenames[i].typename[0] != 0 &&
		    typenames[i].type == type)
			return (&typenames[i]);
	}
	return (NULL);
}

static void
insert_into_typenames(int type, const char *typename, const char *attr) {
	struct ttnam *ttn = NULL;
	size_t c;
	int i, n;
	char tmp[256];

	INSIST(strlen(typename) < TYPECLASSBUF);
	for (i = 0; i < TYPENAMES; i++) {
		if (typenames[i].typename[0] != 0 &&
		    typenames[i].type == type &&
		    strcmp(typename, typenames[i].typename) != 0) {
			fprintf(stderr,
				"Error:  type %d has two names: %s, %s\n",
				type, typenames[i].typename, typename);
			exit(1);
		}
		if (typenames[i].typename[0] == 0 && ttn == NULL)
			ttn = &typenames[i];
	}
	if (ttn == NULL) {
		fprintf(stderr, "Error: typenames array too small\n");
		exit(1);
	}

	/* XXXMUKS: This is redundant due to the INSIST above. */
	if (strlen(typename) > sizeof(ttn->typename) - 1) {
		fprintf(stderr, "Error:  type name %s is too long\n",
			typename);
		exit(1);
	}

	strncpy(ttn->typename, typename, sizeof(ttn->typename));
	ttn->typename[sizeof(ttn->typename) - 1] = '\0';

	strncpy(ttn->macroname, ttn->typename, sizeof(ttn->macroname));
	ttn->macroname[sizeof(ttn->macroname) - 1] = '\0';

	ttn->type = type;
	c = strlen(ttn->macroname);
	while (c > 0) {
		if (ttn->macroname[c - 1] == '-')
			ttn->macroname[c - 1] = '_';
		c--;
	}

	if (attr == NULL) {
		n = snprintf(tmp, sizeof(tmp),
			     "RRTYPE_%s_ATTRIBUTES", upper(ttn->macroname));
		INSIST(n > 0 && (unsigned)n < sizeof(tmp));
		attr = tmp;
	}

	if (ttn->attr[0] != 0 && strcmp(attr, ttn->attr) != 0) {
		fprintf(stderr, "Error:  type %d has different attributes: "
			"%s, %s\n", type, ttn->attr, attr);
		exit(1);
	}

	if (strlen(attr) > sizeof(ttn->attr) - 1) {
		fprintf(stderr, "Error:  attr (%s) [name %s] is too long\n",
			attr, typename);
		exit(1);
	}

	strncpy(ttn->attr, attr, sizeof(ttn->attr));
	ttn->attr[sizeof(ttn->attr) - 1] = '\0';

	ttn->sorted = 0;
	if (maxtype < type)
		maxtype = type;
}

static void
add(int rdclass, const char *classname, int type, const char *typename,
    const char *dirname)
{
	struct tt *newtt = (struct tt *)malloc(sizeof(*newtt));
	struct tt *tt, *oldtt;
	struct cc *newcc;
	struct cc *cc, *oldcc;

	INSIST(strlen(typename) < TYPECLASSBUF);
	INSIST(strlen(classname) < TYPECLASSBUF);
	INSIST(strlen(dirname) < DIRNAMESIZE);

	insert_into_typenames(type, typename, NULL);

	if (newtt == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	newtt->next = NULL;
	newtt->rdclass = rdclass;
	newtt->type = type;

	strncpy(newtt->classname, classname, sizeof(newtt->classname));
	newtt->classname[sizeof(newtt->classname) - 1] = '\0';

	strncpy(newtt->typename, typename, sizeof(newtt->typename));
	newtt->typename[sizeof(newtt->typename) - 1] = '\0';

	if (strncmp(dirname, "./", 2) == 0)
		dirname += 2;
	strncpy(newtt->dirname, dirname, sizeof(newtt->dirname));
	newtt->dirname[sizeof(newtt->dirname) - 1] = '\0';

	tt = types;
	oldtt = NULL;

	while ((tt != NULL) && (tt->type < type)) {
		oldtt = tt;
		tt = tt->next;
	}

	while ((tt != NULL) && (tt->type == type) && (tt->rdclass < rdclass)) {
		if (strcmp(tt->typename, typename) != 0)
			exit(1);
		oldtt = tt;
		tt = tt->next;
	}

	if ((tt != NULL) && (tt->type == type) && (tt->rdclass == rdclass))
		exit(1);

	newtt->next = tt;
	if (oldtt != NULL)
		oldtt->next = newtt;
	else
		types = newtt;

	/*
	 * Do a class switch for this type.
	 */
	if (rdclass == 0)
		return;

	newcc = (struct cc *)malloc(sizeof(*newcc));
	if (newcc == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}
	newcc->rdclass = rdclass;
	strncpy(newcc->classname, classname, sizeof(newcc->classname));
	newcc->classname[sizeof(newcc->classname) - 1] = '\0';
	cc = classes;
	oldcc = NULL;

	while ((cc != NULL) && (cc->rdclass < rdclass)) {
		oldcc = cc;
		cc = cc->next;
	}

	if ((cc != NULL) && cc->rdclass == rdclass) {
		free((char *)newcc);
		return;
	}

	newcc->next = cc;
	if (oldcc != NULL)
		oldcc->next = newcc;
	else
		classes = newcc;
}

static void
sd(int rdclass, const char *classname, const char *dirname, char filetype) {
	char buf[TYPECLASSLEN + sizeof("_65535.h")];
	char typename[TYPECLASSBUF];
	int type, n;
	isc_dir_t dir;

	if (!start_directory(dirname, &dir))
		return;

	while (next_file(&dir)) {
		if (sscanf(dir.filename, TYPECLASSFMT, typename, &type) != 2)
			continue;
		if ((type > 65535) || (type < 0))
			continue;

		n = snprintf(buf, sizeof(buf), "%s_%d.%c", typename,
			     type, filetype);
		INSIST(n > 0 && (unsigned)n < sizeof(buf));
		if (strcmp(buf, dir.filename) != 0)
			continue;
		add(rdclass, classname, type, typename, dirname);
	}

	end_directory(&dir);
}

static unsigned int
HASH(char *string) {
	size_t n;
	unsigned char a, b;

	n = strlen(string);
	if (n == 0) {
		fprintf(stderr, "n == 0?\n");
		exit(1);
	}
	a = tolower((unsigned char)string[0]);
	b = tolower((unsigned char)string[n - 1]);

	return ((a + n) * b) % 256;
}

int
main(int argc, char **argv) {
	char buf[DIRNAMESIZE];		/* XXX Should be max path length */
	char srcdir[DIRNAMESIZE];	/* XXX Should be max path length */
	int rdclass;
	char classname[TYPECLASSBUF];
	struct tt *tt;
	struct cc *cc;
	struct ttnam *ttn, *ttn2;
	unsigned int hash;
	struct tm *tm;
	time_t now;
	char year[11];
	int lasttype;
	int code = 1;
	int class_enum = 0;
	int type_enum = 0;
	int structs = 0;
	int depend = 0;
	int c, i, j, n;
	char buf1[TYPECLASSBUF];
	char filetype = 'c';
	FILE *fd;
	char *prefix = NULL;
	char *suffix = NULL;
	char *file = NULL;
	isc_dir_t dir;

	for (i = 0; i < TYPENAMES; i++)
		memset(&typenames[i], 0, sizeof(typenames[i]));

	strcpy(srcdir, "");
	while ((c = isc_commandline_parse(argc, argv, "cdits:F:P:S:")) != -1)
		switch (c) {
		case 'c':
			code = 0;
			depend = 0;
			type_enum = 0;
			class_enum = 1;
			filetype = 'c';
			structs = 0;
			break;
		case 'd':
			code = 0;
			depend = 1;
			class_enum = 0;
			type_enum = 0;
			structs = 0;
			filetype = 'h';
			break;
		case 't':
			code = 0;
			depend = 0;
			class_enum = 0;
			type_enum = 1;
			filetype = 'c';
			structs = 0;
			break;
		case 'i':
			code = 0;
			depend = 0;
			class_enum = 0;
			type_enum = 0;
			structs = 1;
			filetype = 'h';
			break;
		case 's':
			if (strlen(isc_commandline_argument) >
			    DIRNAMESIZE - 2 * TYPECLASSLEN  -
			    sizeof("/rdata/_65535_65535")) {
				fprintf(stderr, "\"%s\" too long\n",
					isc_commandline_argument);
				exit(1);
			}
			n = snprintf(srcdir, sizeof(srcdir), "%s/",
				     isc_commandline_argument);
			INSIST(n > 0 && (unsigned)n < sizeof(srcdir));
			break;
		case 'F':
			file = isc_commandline_argument;
			break;
		case 'P':
			prefix = isc_commandline_argument;
			break;
		case 'S':
			suffix = isc_commandline_argument;
			break;
		case '?':
			exit(1);
		}

	n = snprintf(buf, sizeof(buf), "%srdata", srcdir);
	INSIST(n > 0 && (unsigned)n < sizeof(srcdir));

	if (!start_directory(buf, &dir))
		exit(1);

	while (next_file(&dir)) {
		if (sscanf(dir.filename, TYPECLASSFMT, classname,
			   &rdclass) != 2)
			continue;
		if ((rdclass > 65535) || (rdclass < 0))
			continue;

		n = snprintf(buf, sizeof(buf), "%srdata/%s_%d",
			     srcdir, classname, rdclass);
		INSIST(n > 0 && (unsigned)n < sizeof(buf));
		if (strcmp(buf + 6 + strlen(srcdir), dir.filename) != 0)
			continue;
		sd(rdclass, classname, buf, filetype);
	}
	end_directory(&dir);
	n = snprintf(buf, sizeof(buf), "%srdata/generic", srcdir);
	INSIST(n > 0 && (unsigned)n < sizeof(srcdir));
	sd(0, "", buf, filetype);

	if (time(&now) != -1) {
		if ((tm = localtime(&now)) != NULL && tm->tm_year > 104) {
			n = snprintf(year, sizeof(year), "-%d",
				     tm->tm_year + 1900);
			INSIST(n > 0 && (unsigned)n < sizeof(year));
		} else
			year[0] = 0;
	} else
		year[0] = 0;

	if (!depend) fprintf(stdout, copyright, year);

	if (code) {
		fputs("#ifndef DNS_CODE_H\n", stdout);
		fputs("#define DNS_CODE_H 1\n\n", stdout);

		fputs("#include <isc/boolean.h>\n", stdout);
		fputs("#include <isc/result.h>\n\n", stdout);
		fputs("#include <dns/name.h>\n\n", stdout);

		for (tt = types; tt != NULL; tt = tt->next)
			fprintf(stdout, "#include \"%s/%s_%d.c\"\n",
				tt->dirname, tt->typename, tt->type);

		fputs("\n\n", stdout);

		doswitch("FROMTEXTSWITCH", "fromtext", FROMTEXTARGS,
			 FROMTEXTTYPE, FROMTEXTCLASS, FROMTEXTDEF);
		doswitch("TOTEXTSWITCH", "totext", TOTEXTARGS,
			 TOTEXTTYPE, TOTEXTCLASS, TOTEXTDEF);
		doswitch("FROMWIRESWITCH", "fromwire", FROMWIREARGS,
			 FROMWIRETYPE, FROMWIRECLASS, FROMWIREDEF);
		doswitch("TOWIRESWITCH", "towire", TOWIREARGS,
			 TOWIRETYPE, TOWIRECLASS, TOWIREDEF);
		doswitch("COMPARESWITCH", "compare", COMPAREARGS,
			  COMPARETYPE, COMPARECLASS, COMPAREDEF);
		doswitch("CASECOMPARESWITCH", "casecompare", COMPAREARGS,
			  COMPARETYPE, COMPARECLASS, COMPAREDEF);
		doswitch("FROMSTRUCTSWITCH", "fromstruct", FROMSTRUCTARGS,
			  FROMSTRUCTTYPE, FROMSTRUCTCLASS, FROMSTRUCTDEF);
		doswitch("TOSTRUCTSWITCH", "tostruct", TOSTRUCTARGS,
			  TOSTRUCTTYPE, TOSTRUCTCLASS, TOSTRUCTDEF);
		doswitch("FREESTRUCTSWITCH", "freestruct", FREESTRUCTARGS,
			  FREESTRUCTTYPE, FREESTRUCTCLASS, FREESTRUCTDEF);
		doswitch("ADDITIONALDATASWITCH", "additionaldata",
			 ADDITIONALDATAARGS, ADDITIONALDATATYPE,
			 ADDITIONALDATACLASS, ADDITIONALDATADEF);
		doswitch("DIGESTSWITCH", "digest",
			 DIGESTARGS, DIGESTTYPE,
			 DIGESTCLASS, DIGESTDEF);
		doswitch("CHECKOWNERSWITCH", "checkowner",
			CHECKOWNERARGS, CHECKOWNERTYPE,
			CHECKOWNERCLASS, CHECKOWNERDEF);
		doswitch("CHECKNAMESSWITCH", "checknames",
			CHECKNAMESARGS, CHECKNAMESTYPE,
			CHECKNAMESCLASS, CHECKNAMESDEF);

		/*
		 * From here down, we are processing the rdata names and
		 * attributes.
		 */

#define PRINT_COMMA(x) (x == maxtype ? "" : ",")

#define METANOTQUESTION  "DNS_RDATATYPEATTR_META | " \
			 "DNS_RDATATYPEATTR_NOTQUESTION"
#define METAQUESTIONONLY "DNS_RDATATYPEATTR_META | " \
			 "DNS_RDATATYPEATTR_QUESTIONONLY"
#define RESERVED "DNS_RDATATYPEATTR_RESERVED"

		/*
		 * Add in reserved/special types.  This will let us
		 * sort them without special cases.
		 */
		insert_into_typenames(0, "reserved0", RESERVED);
		insert_into_typenames(31, "eid", RESERVED);
		insert_into_typenames(32, "nimloc", RESERVED);
		insert_into_typenames(34, "atma", RESERVED);
		insert_into_typenames(100, "uinfo", RESERVED);
		insert_into_typenames(101, "uid", RESERVED);
		insert_into_typenames(102, "gid", RESERVED);
		insert_into_typenames(251, "ixfr", METAQUESTIONONLY);
		insert_into_typenames(252, "axfr", METAQUESTIONONLY);
		insert_into_typenames(253, "mailb", METAQUESTIONONLY);
		insert_into_typenames(254, "maila", METAQUESTIONONLY);
		insert_into_typenames(255, "any", METAQUESTIONONLY);

		/*
		 * Spit out a quick and dirty hash function.  Here,
		 * we walk through the list of type names, and calculate
		 * a hash.  This isn't perfect, but it will generate "pretty
		 * good" estimates.  Lowercase the characters before
		 * computing in all cases.
		 *
		 * Here, walk the list from top to bottom, calculating
		 * the hash (mod 256) for each name.
		 */
		fprintf(stdout, "#define RDATATYPE_COMPARE(_s, _d, _tn, _n, _tp) \\\n");
		fprintf(stdout, "\tdo { \\\n");
		fprintf(stdout, "\t\tif (sizeof(_s) - 1 == _n && \\\n"
				"\t\t    strncasecmp(_s,(_tn),"
				"(sizeof(_s) - 1)) == 0) { \\\n");
		fprintf(stdout, "\t\t\tif ((dns_rdatatype_attributes(_d) & "
				  "DNS_RDATATYPEATTR_RESERVED) != 0) \\\n");
		fprintf(stdout, "\t\t\t\treturn (ISC_R_NOTIMPLEMENTED); \\\n");
		fprintf(stdout, "\t\t\t*(_tp) = _d; \\\n");
		fprintf(stdout, "\t\t\treturn (ISC_R_SUCCESS); \\\n");
		fprintf(stdout, "\t\t} \\\n");
		fprintf(stdout, "\t} while (0)\n\n");

		fprintf(stdout, "#define RDATATYPE_FROMTEXT_SW(_hash,"
				"_typename,_length,_typep) \\\n");
		fprintf(stdout, "\tswitch (_hash) { \\\n");
		for (i = 0; i <= maxtype; i++) {
			ttn = find_typename(i);
			if (ttn == NULL)
				continue;

			/*
			 * Skip entries we already processed.
			 */
			if (ttn->sorted != 0)
				continue;

			hash = HASH(ttn->typename);
			fprintf(stdout, "\t\tcase %u: \\\n", hash);

			/*
			 * Find all other entries that happen to match
			 * this hash.
			 */
			for (j = 0; j <= maxtype; j++) {
				ttn2 = find_typename(j);
				if (ttn2 == NULL)
					continue;
				if (hash == HASH(ttn2->typename)) {
					fprintf(stdout, "\t\t\tRDATATYPE_COMPARE"
					       "(\"%s\", %u, "
					       "_typename, _length, _typep); \\\n",
					       ttn2->typename, ttn2->type);
					ttn2->sorted = 1;
				}
			}
			fprintf(stdout, "\t\t\tbreak; \\\n");
		}
		fprintf(stdout, "\t}\n");

		fprintf(stdout, "#define RDATATYPE_ATTRIBUTE_SW \\\n");
		fprintf(stdout, "\tswitch (type) { \\\n");
		for (i = 0; i <= maxtype; i++) {
			ttn = find_typename(i);
			if (ttn == NULL)
				continue;
			fprintf(stdout, "\tcase %u: return (%s); \\\n",
				i, upper(ttn->attr));
		}
		fprintf(stdout, "\t}\n");

		fprintf(stdout, "#define RDATATYPE_TOTEXT_SW \\\n");
		fprintf(stdout, "\tswitch (type) { \\\n");
		for (i = 0; i <= maxtype; i++) {
			ttn = find_typename(i);
			if (ttn == NULL)
				continue;
			/*
			 * Remove KEYDATA (65533) from the type to memonic
			 * translation as it is internal use only.  This
			 * stops the tools from displaying KEYDATA instead
			 * of TYPE65533.
			 */
			if (i == 65533U)
				continue;
			fprintf(stdout, "\tcase %u: return "
				"(str_totext(\"%s\", target)); \\\n",
				i, upper(ttn->typename));
		}
		fprintf(stdout, "\t}\n");

		fputs("#endif /* DNS_CODE_H */\n", stdout);
	} else if (type_enum) {
		char *s;

		fprintf(stdout, "#ifndef DNS_ENUMTYPE_H\n");
		fprintf(stdout, "#define DNS_ENUMTYPE_H 1\n\n");

		fprintf(stdout, "enum {\n");
		fprintf(stdout, "\tdns_rdatatype_none = 0,\n");

		lasttype = 0;
		for (tt = types; tt != NULL; tt = tt->next)
			if (tt->type != lasttype)
				fprintf(stdout,
					"\tdns_rdatatype_%s = %d,\n",
					funname(tt->typename, buf1),
					lasttype = tt->type);

		fprintf(stdout, "\tdns_rdatatype_ixfr = 251,\n");
		fprintf(stdout, "\tdns_rdatatype_axfr = 252,\n");
		fprintf(stdout, "\tdns_rdatatype_mailb = 253,\n");
		fprintf(stdout, "\tdns_rdatatype_maila = 254,\n");
		fprintf(stdout, "\tdns_rdatatype_any = 255\n");

		fprintf(stdout, "};\n\n");

		fprintf(stdout, "#define dns_rdatatype_none\t"
			"((dns_rdatatype_t)dns_rdatatype_none)\n");

		for (tt = types; tt != NULL; tt = tt->next)
			if (tt->type != lasttype) {
				s = funname(tt->typename, buf1);
				fprintf(stdout,
					"#define dns_rdatatype_%s\t%s"
					"((dns_rdatatype_t)dns_rdatatype_%s)"
					"\n",
					s, strlen(s) < 2U ? "\t" : "", s);
				lasttype = tt->type;
			}

		fprintf(stdout, "#define dns_rdatatype_ixfr\t"
			"((dns_rdatatype_t)dns_rdatatype_ixfr)\n");
		fprintf(stdout, "#define dns_rdatatype_axfr\t"
			"((dns_rdatatype_t)dns_rdatatype_axfr)\n");
		fprintf(stdout, "#define dns_rdatatype_mailb\t"
			"((dns_rdatatype_t)dns_rdatatype_mailb)\n");
		fprintf(stdout, "#define dns_rdatatype_maila\t"
			"((dns_rdatatype_t)dns_rdatatype_maila)\n");
		fprintf(stdout, "#define dns_rdatatype_any\t"
			"((dns_rdatatype_t)dns_rdatatype_any)\n");

		fprintf(stdout, "\n#endif /* DNS_ENUMTYPE_H */\n");

	} else if (class_enum) {
		char *s;
		int classnum;

		fprintf(stdout, "#ifndef DNS_ENUMCLASS_H\n");
		fprintf(stdout, "#define DNS_ENUMCLASS_H 1\n\n");

		fprintf(stdout, "enum {\n");

		fprintf(stdout, "\tdns_rdataclass_reserved0 = 0,\n");
		fprintf(stdout, "#define dns_rdataclass_reserved0 \\\n\t\t\t\t"
			"((dns_rdataclass_t)dns_rdataclass_reserved0)\n");

#define PRINTCLASS(name, num) \
	do { \
		s = funname(name, buf1); \
		classnum = num; \
		fprintf(stdout, "\tdns_rdataclass_%s = %d%s\n", s, classnum, \
		       classnum != 255 ? "," : ""); \
		fprintf(stdout, "#define dns_rdataclass_%s\t" \
		       "((dns_rdataclass_t)dns_rdataclass_%s)\n", s, s); \
	} while (0)

		for (cc = classes; cc != NULL; cc = cc->next) {
			if (cc->rdclass == 3)
				PRINTCLASS("chaos", 3);
			else if (cc->rdclass == 255)
				PRINTCLASS("none", 254);
			PRINTCLASS(cc->classname, cc->rdclass);
		}

#undef PRINTCLASS

		fprintf(stdout, "};\n\n");
		fprintf(stdout, "#endif /* DNS_ENUMCLASS_H */\n");
	} else if (structs) {
		if (prefix != NULL) {
			if ((fd = fopen(prefix,"r")) != NULL) {
				while (fgets(buf, sizeof(buf), fd) != NULL)
					fputs(buf, stdout);
				fclose(fd);
			}
		}
		for (tt = types; tt != NULL; tt = tt->next) {
			snprintf(buf, sizeof(buf), "%s/%s_%d.h",
				tt->dirname, tt->typename, tt->type);
			if ((fd = fopen(buf,"r")) != NULL) {
				while (fgets(buf, sizeof(buf), fd) != NULL)
					fputs(buf, stdout);
				fclose(fd);
			}
		}
		if (suffix != NULL) {
			if ((fd = fopen(suffix,"r")) != NULL) {
				while (fgets(buf, sizeof(buf), fd) != NULL)
					fputs(buf, stdout);
				fclose(fd);
			}
		}
	} else if (depend) {
		for (tt = types; tt != NULL; tt = tt->next)
			fprintf(stdout, "%s:\t%s/%s_%d.h\n", file,
				tt->dirname, tt->typename, tt->type);
	}

	if (ferror(stdout) != 0)
		exit(1);

	return (0);
}
