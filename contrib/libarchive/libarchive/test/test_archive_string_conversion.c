/*-
 * Copyright (c) 2011 Michihiro NAKAJIMA
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

#include <locale.h>

#define __LIBARCHIVE_TEST
#include "archive_string.h"

/*
Execute the following to rebuild the data for this program:
   tail -n +36 test_archive_string_conversion.c | /bin/sh
#
# This requires http://unicode.org/Public/UNIDATA/NormalizationTest.txt
#
if="NormalizationTest.txt"
if [ ! -f ${if} ]; then
  echo "Not found: \"${if}\""
  exit 0
fi
of=test_archive_string_conversion.txt.Z
echo "\$FreeBSD\$" > ${of}.uu
awk -F ';'  '$0 ~/^[0-9A-F]+/ {printf "%s;%s\n", $2, $3}' ${if} | compress | uuencode ${of} >> ${of}.uu
exit 1
*/

static int
unicode_to_utf8(char *p, uint32_t uc)
{        
        char *_p = p;

        /* Translate code point to UTF8 */
        if (uc <= 0x7f) {
                *p++ = (char)uc;
        } else if (uc <= 0x7ff) {
                *p++ = 0xc0 | ((uc >> 6) & 0x1f);
                *p++ = 0x80 | (uc & 0x3f);
        } else if (uc <= 0xffff) {
                *p++ = 0xe0 | ((uc >> 12) & 0x0f);
                *p++ = 0x80 | ((uc >> 6) & 0x3f);
                *p++ = 0x80 | (uc & 0x3f);
        } else {
                *p++ = 0xf0 | ((uc >> 18) & 0x07);
                *p++ = 0x80 | ((uc >> 12) & 0x3f);
                *p++ = 0x80 | ((uc >> 6) & 0x3f);
                *p++ = 0x80 | (uc & 0x3f);
        }
        return ((int)(p - _p));
}

static void
archive_be16enc(void *pp, uint16_t u)
{
        unsigned char *p = (unsigned char *)pp;

        p[0] = (u >> 8) & 0xff;
        p[1] = u & 0xff;
}

static int
unicode_to_utf16be(char *p, uint32_t uc)
{
	char *utf16 = p;

	if (uc > 0xffff) {
		/* We have a code point that won't fit into a
		 * wchar_t; convert it to a surrogate pair. */
		uc -= 0x10000;
		archive_be16enc(utf16, ((uc >> 10) & 0x3ff) + 0xD800);
		archive_be16enc(utf16+2, (uc & 0x3ff) + 0xDC00);
		return (4);
	} else {
		archive_be16enc(utf16, uc);
		return (2);
	}
}

static void
archive_le16enc(void *pp, uint16_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
}

static size_t
unicode_to_utf16le(char *p, uint32_t uc)
{
	char *utf16 = p;

	if (uc > 0xffff) {
		/* We have a code point that won't fit into a
		 * wchar_t; convert it to a surrogate pair. */
		uc -= 0x10000;
		archive_le16enc(utf16, ((uc >> 10) & 0x3ff) + 0xD800);
		archive_le16enc(utf16+2, (uc & 0x3ff) + 0xDC00);
		return (4);
	} else {
		archive_le16enc(utf16, uc);
		return (2);
	}
}

static int
wc_size(void)
{
	return (sizeof(wchar_t));
}

static int
unicode_to_wc(wchar_t *wp, uint32_t uc)
{
	if (wc_size() == 4) {
		*wp = (wchar_t)uc;
		return (1);
	} 
	if (uc > 0xffff) {
		/* We have a code point that won't fit into a
		 * wchar_t; convert it to a surrogate pair. */
		uc -= 0x10000;
		*wp++ = (wchar_t)(((uc >> 10) & 0x3ff) + 0xD800);
		*wp = (wchar_t)((uc & 0x3ff) + 0xDC00);
		return (2);
	} else {
		*wp = (wchar_t)uc;
		return (1);
	}
}

/*
 * Note: U+2000 - U+2FFF, U+F900 - U+FAFF and U+2F800 - U+2FAFF are not
 * converted to NFD on Mac OS.
 * see also http://developer.apple.com/library/mac/#qa/qa2001/qa1173.html
 */
static int
scan_unicode_pattern(char *out, wchar_t *wout, char *u16be, char *u16le,
    const char *pattern, int exclude_mac_nfd)
{
	unsigned uc = 0;
	const char *p = pattern;
	char *op = out;
	wchar_t *owp = wout;
	char *op16be = u16be;
	char *op16le = u16le;

	for (;;) {
		if (*p >= '0' && *p <= '9')
			uc = (uc << 4) + (*p - '0');
		else if (*p >= 'A' && *p <= 'F')
			uc = (uc << 4) + (*p - 'A' + 0x0a);
		else {
			if (exclude_mac_nfd) {
				/*
				 * These are not converted to NFD on Mac OS.
				 */
				if ((uc >= 0x2000 && uc <= 0x2FFF) ||
				    (uc >= 0xF900 && uc <= 0xFAFF) ||
				    (uc >= 0x2F800 && uc <= 0x2FAFF))
					return (-1);
				/*
				 * Those code points are not converted to
				 * NFD on Mac OS. I do not know the reason
				 * because it is undocumented.
				 *   NFC        NFD
				 *   1109A  ==> 11099 110BA
				 *   1109C  ==> 1109B 110BA
				 *   110AB  ==> 110A5 110BA
				 */
				if (uc == 0x1109A || uc == 0x1109C ||
				    uc == 0x110AB)
					return (-1);
			}
			op16be += unicode_to_utf16be(op16be, uc);
			op16le += unicode_to_utf16le(op16le, uc);
			owp += unicode_to_wc(owp, uc);
			op += unicode_to_utf8(op, uc);
			if (!*p) {
				*op16be++ = 0;
				*op16be = 0;
				*op16le++ = 0;
				*op16le = 0;
				*owp = L'\0';
				*op = '\0';
				break;
			}
			uc = 0;
		}
		p++;
	}
	return (0);
}

static int
is_wc_unicode(void)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	return (1);
#else
	return (0);
#endif
}

/*
 * A conversion test that we correctly normalize UTF-8 and UTF-16BE characters.
 * On Mac OS, the characters to be Form D.
 * On other platforms, the characters to be Form C.
 */
static void
test_archive_string_normalization(void)
{
	struct archive *a, *a2;
	struct archive_entry *ae;
	struct archive_string utf8;
	struct archive_mstring mstr;
	struct archive_string_conv *f_sconv8, *t_sconv8;
	struct archive_string_conv *f_sconv16be, *f_sconv16le;
	FILE *fp;
	char buff[512];
	static const char reffile[] = "test_archive_string_conversion.txt.Z";
	ssize_t size;
	int line = 0;
	int locale_is_utf8, wc_is_unicode;

	locale_is_utf8 = (NULL != setlocale(LC_ALL, "en_US.UTF-8"));
	wc_is_unicode = is_wc_unicode();
	/* If it doesn't exist, just warn and return. */
	if (!locale_is_utf8 && !wc_is_unicode) {
		skipping("invalid encoding tests require a suitable locale;"
		    " en_US.UTF-8 not available on this system");
		return;
	}

	archive_string_init(&utf8);
	memset(&mstr, 0, sizeof(mstr));

	/*
	 * Extract a test pattern file.
	 */
	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
        assertEqualIntA(a, ARCHIVE_OK,
            archive_read_open_filename(a, reffile, 512));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assert((fp = fopen("testdata.txt", "w")) != NULL);
	while ((size = archive_read_data(a, buff, 512)) > 0)
		fwrite(buff, 1, size, fp);
	fclose(fp);

	/* Open a test pattern file. */
	assert((fp = fopen("testdata.txt", "r")) != NULL);

	/*
	 * Create string conversion objects.
	 */
	assertA(NULL != (f_sconv8 =
	    archive_string_conversion_from_charset(a, "UTF-8", 0)));
	assertA(NULL != (f_sconv16be =
	    archive_string_conversion_from_charset(a, "UTF-16BE", 0)));
	assertA(NULL != (f_sconv16le =
	    archive_string_conversion_from_charset(a, "UTF-16LE", 0)));
	assert((a2 = archive_write_new()) != NULL);
	assertA(NULL != (t_sconv8 =
	    archive_string_conversion_to_charset(a2, "UTF-8", 0)));
	if (f_sconv8 == NULL || f_sconv16be == NULL || f_sconv16le == NULL ||
	    t_sconv8 == NULL || fp == NULL) {
		/* We cannot continue this test. */
		if (fp != NULL)
			fclose(fp);
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}

	/*
	 * Read test data.
	 *  Test data format:
	 *     <NFC Unicode pattern> ';' <NFD Unicode pattern> '\n'
	 *  Unicode pattern format:
	 *     [0-9A-F]{4,5}([ ][0-9A-F]{4,5}){0,}
	 */
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		char nfc[80], nfd[80];
		char utf8_nfc[80], utf8_nfd[80];
		char utf16be_nfc[80], utf16be_nfd[80];
		char utf16le_nfc[80], utf16le_nfd[80];
		wchar_t wc_nfc[40], wc_nfd[40];
		char *e, *p;

		line++;
		if (buff[0] == '#')
			continue;
		p = strchr(buff, ';');
		if (p == NULL)
			continue;
		*p++ = '\0';
		/* Copy an NFC pattern */
		strncpy(nfc, buff, sizeof(nfc)-1);
		nfc[sizeof(nfc)-1] = '\0';
		e = p;
		p = strchr(p, '\n');
		if (p == NULL)
			continue;
		*p = '\0';
		/* Copy an NFD pattern */
		strncpy(nfd, e, sizeof(nfd)-1);
		nfd[sizeof(nfd)-1] = '\0';

		/*
		 * Convert an NFC pattern to UTF-8 bytes.
		 */
#if defined(__APPLE__)
		if (scan_unicode_pattern(utf8_nfc, wc_nfc, utf16be_nfc, utf16le_nfc,
		    nfc, 1) != 0)
			continue;
#else
		scan_unicode_pattern(utf8_nfc, wc_nfc, utf16be_nfc, utf16le_nfc,
		    nfc, 0);
#endif

		/*
		 * Convert an NFD pattern to UTF-8 bytes.
		 */
		scan_unicode_pattern(utf8_nfd, wc_nfd, utf16be_nfd, utf16le_nfd,
		    nfd, 0);

		if (locale_is_utf8) {
#if defined(__APPLE__)
			/*
			 * Normalize an NFC string for import.
			 */
			assertEqualInt(0, archive_strcpy_in_locale(
			    &utf8, utf8_nfc, f_sconv8));
			failure("NFC(%s) should be converted to NFD(%s):%d",
			    nfc, nfd, line);
			assertEqualUTF8String(utf8_nfd, utf8.s);

			/*
			 * Normalize an NFD string for import.
			 */
			assertEqualInt(0, archive_strcpy_in_locale(
			    &utf8, utf8_nfd, f_sconv8));
			failure("NFD(%s) should not be any changed:%d",
			    nfd, line);
			assertEqualUTF8String(utf8_nfd, utf8.s);

			/*
			 * Copy an NFD string for export.
			 */
			assertEqualInt(0, archive_strcpy_in_locale(
			    &utf8, utf8_nfd, t_sconv8));
			failure("NFD(%s) should not be any changed:%d",
			    nfd, line);
			assertEqualUTF8String(utf8_nfd, utf8.s);

			/*
			 * Normalize an NFC string in UTF-16BE for import.
			 */
			assertEqualInt(0, archive_strncpy_in_locale(
			    &utf8, utf16be_nfc, 100000, f_sconv16be));
			failure("NFC(%s) should be converted to NFD(%s):%d",
			    nfc, nfd, line);
			assertEqualUTF8String(utf8_nfd, utf8.s);

			/*
			 * Normalize an NFC string in UTF-16LE for import.
			 */
			assertEqualInt(0, archive_strncpy_in_locale(
			    &utf8, utf16le_nfc, 100000, f_sconv16le));
			failure("NFC(%s) should be converted to NFD(%s):%d",
			    nfc, nfd, line);
			assertEqualUTF8String(utf8_nfd, utf8.s);
#else
			/*
			 * Normalize an NFD string for import.
			 */
			assertEqualInt(0, archive_strcpy_in_locale(
			    &utf8, utf8_nfd, f_sconv8));
			failure("NFD(%s) should be converted to NFC(%s):%d",
			    nfd, nfc, line);
			assertEqualUTF8String(utf8_nfc, utf8.s);

			/*
			 * Normalize an NFC string for import.
			 */
			assertEqualInt(0, archive_strcpy_in_locale(
			    &utf8, utf8_nfc, f_sconv8));
			failure("NFC(%s) should not be any changed:%d",
			    nfc, line);
			assertEqualUTF8String(utf8_nfc, utf8.s);

			/*
			 * Copy an NFC string for export.
			 */
			assertEqualInt(0, archive_strcpy_in_locale(
			    &utf8, utf8_nfc, t_sconv8));
			failure("NFC(%s) should not be any changed:%d",
			    nfc, line);
			assertEqualUTF8String(utf8_nfc, utf8.s);

			/*
			 * Normalize an NFD string in UTF-16BE for import.
			 */
			assertEqualInt(0, archive_strncpy_in_locale(
			    &utf8, utf16be_nfd, 100000, f_sconv16be));
			failure("NFD(%s) should be converted to NFC(%s):%d",
			    nfd, nfc, line);
			assertEqualUTF8String(utf8_nfc, utf8.s);

			/*
			 * Normalize an NFD string in UTF-16LE for import.
			 */
			assertEqualInt(0, archive_strncpy_in_locale(
			    &utf8, utf16le_nfd, 100000, f_sconv16le));
			failure("NFD(%s) should be converted to NFC(%s):%d",
			    nfd, nfc, line);
			assertEqualUTF8String(utf8_nfc, utf8.s);
#endif
		}

		/*
		 * Test for archive_mstring interface.
		 * In specific, Windows platform UTF-16BE is directly
		 * converted to/from wide-character to avoid the effect of
		 * current locale since windows platform cannot make
		 * locale UTF-8.
		 */
		if (locale_is_utf8 || wc_is_unicode) {
			const wchar_t *wp;
			const char *mp;
			size_t mplen;

#if defined(__APPLE__)
			/*
			 * Normalize an NFD string in UTF-8 for import.
			 */
			assertEqualInt(0, archive_mstring_copy_mbs_len_l(
			    &mstr, utf8_nfc, 100000, f_sconv8));
			assertEqualInt(0,
			    archive_mstring_get_wcs(a, &mstr, &wp));
			failure("UTF-8 NFC(%s) should be converted "
			    "to WCS NFD(%s):%d", nfc, nfd, line);
			assertEqualWString(wc_nfd, wp);

			/*
			 * Normalize an NFD string in UTF-16BE for import.
			 */
			assertEqualInt(0, archive_mstring_copy_mbs_len_l(
			    &mstr, utf16be_nfc, 100000, f_sconv16be));
			assertEqualInt(0,
			    archive_mstring_get_wcs(a, &mstr, &wp));
			failure("UTF-16BE NFC(%s) should be converted "
			    "to WCS NFD(%s):%d", nfc, nfd, line);
			assertEqualWString(wc_nfd, wp);

			/*
			 * Normalize an NFD string in UTF-16LE for import.
			 */
			assertEqualInt(0, archive_mstring_copy_mbs_len_l(
			    &mstr, utf16le_nfc, 100000, f_sconv16le));
			assertEqualInt(0,
			    archive_mstring_get_wcs(a, &mstr, &wp));
			failure("UTF-16LE NFC(%s) should be converted "
			    "to WCS NFD(%s):%d", nfc, nfd, line);
			assertEqualWString(wc_nfd, wp);

			/*
			 * Copy an NFD wide-string for export.
			 */
			assertEqualInt(0, archive_mstring_copy_wcs(
			    &mstr, wc_nfd));
			assertEqualInt(0, archive_mstring_get_mbs_l(
			    &mstr, &mp, &mplen, t_sconv8));
			failure("WCS NFD(%s) should be UTF-8 NFD:%d"
			    ,nfd, line);
			assertEqualUTF8String(utf8_nfd, mp);
#else
			/*
			 * Normalize an NFD string in UTF-8 for import.
			 */
			assertEqualInt(0, archive_mstring_copy_mbs_len_l(
			    &mstr, utf8_nfd, 100000, f_sconv8));
			assertEqualInt(0,
			    archive_mstring_get_wcs(a, &mstr, &wp));
			failure("UTF-8 NFD(%s) should be converted "
			    "to WCS NFC(%s):%d", nfd, nfc, line);
			assertEqualWString(wc_nfc, wp);

			/*
			 * Normalize an NFD string in UTF-16BE for import.
			 */
			assertEqualInt(0, archive_mstring_copy_mbs_len_l(
			    &mstr, utf16be_nfd, 100000, f_sconv16be));
			assertEqualInt(0,
			    archive_mstring_get_wcs(a, &mstr, &wp));
			failure("UTF-8 NFD(%s) should be converted "
			    "to WCS NFC(%s):%d", nfd, nfc, line);
			assertEqualWString(wc_nfc, wp);

			/*
			 * Normalize an NFD string in UTF-16LE for import.
			 */
			assertEqualInt(0, archive_mstring_copy_mbs_len_l(
			    &mstr, utf16le_nfd, 100000, f_sconv16le));
			assertEqualInt(0,
			    archive_mstring_get_wcs(a, &mstr, &wp));
			failure("UTF-8 NFD(%s) should be converted "
			    "to WCS NFC(%s):%d", nfd, nfc, line);
			assertEqualWString(wc_nfc, wp);

			/*
			 * Copy an NFC wide-string for export.
			 */
			assertEqualInt(0, archive_mstring_copy_wcs(
			    &mstr, wc_nfc));
			assertEqualInt(0, archive_mstring_get_mbs_l(
			    &mstr, &mp, &mplen, t_sconv8));
			failure("WCS NFC(%s) should be UTF-8 NFC:%d"
			    ,nfc, line);
			assertEqualUTF8String(utf8_nfc, mp);
#endif
		}
	}

	archive_string_free(&utf8);
	archive_mstring_clean(&mstr);
	fclose(fp);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a2));
}

static void
test_archive_string_canonicalization(void)
{
	struct archive *a;
	struct archive_string_conv *sconv;

	setlocale(LC_ALL, "en_US.UTF-8");

	assert((a = archive_read_new()) != NULL);

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "UTF-8", 1)));
	failure("Charset name should be UTF-8");
	assertEqualString("UTF-8",
	    archive_string_conversion_charset_name(sconv));

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "UTF8", 1)));
	failure("Charset name should be UTF-8");
	assertEqualString("UTF-8",
	    archive_string_conversion_charset_name(sconv));

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "utf8", 1)));
	failure("Charset name should be UTF-8");
	assertEqualString("UTF-8",
	    archive_string_conversion_charset_name(sconv));

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "UTF-16BE", 1)));
	failure("Charset name should be UTF-16BE");
	assertEqualString("UTF-16BE",
	    archive_string_conversion_charset_name(sconv));

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "UTF16BE", 1)));
	failure("Charset name should be UTF-16BE");
	assertEqualString("UTF-16BE",
	    archive_string_conversion_charset_name(sconv));

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "utf16be", 1)));
	failure("Charset name should be UTF-16BE");
	assertEqualString("UTF-16BE",
	    archive_string_conversion_charset_name(sconv));

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "UTF-16LE", 1)));
	failure("Charset name should be UTF-16LE");
	assertEqualString("UTF-16LE",
	    archive_string_conversion_charset_name(sconv));

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "UTF16LE", 1)));
	failure("Charset name should be UTF-16LE");
	assertEqualString("UTF-16LE",
	    archive_string_conversion_charset_name(sconv));

	assertA(NULL != (sconv =
	    archive_string_conversion_to_charset(a, "utf16le", 1)));
	failure("Charset name should be UTF-16LE");
	assertEqualString("UTF-16LE",
	    archive_string_conversion_charset_name(sconv));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

}

DEFINE_TEST(test_archive_string_conversion)
{
	test_archive_string_normalization();
	test_archive_string_canonicalization();
}
