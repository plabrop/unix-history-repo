/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)ctype_case.c	5.1 (Berkeley) %G%";
#endif /* LIBC_SCCS and not lint */

char __maplower[1 + 256] = {
	0,
	0000,	0001,	0002,	0003,	0004,	0005,	0006,	0007,
	0010,	0011,	0012,	0013,	0014,	0015,	0016,	0017,
	0020,	0021,	0022,	0023,	0024,	0025,	0026,	0027,
	0030,	0031,	0032,	0033,	0034,	0035,	0036,	0037,
	0040,	0041,	0042,	0043,	0044,	0045,	0046,	0047,
	0050,	0051,	0052,	0053,	0054,	0055,	0056,	0057,
	0060,	0061,	0062,	0063,	0064,	0065,	0066,	0067,
	0070,	0071,	0072,	0073,	0074,	0075,	0076,	0077,
	0100,	 'a',	 'b',	 'c',	 'd',	 'e',	 'f',	 'g',
	 'h',	 'i',	 'j',	 'k',	 'l',	 'm',	 'n',	 'o',
	 'p',	 'q',	 'r',	 's',	 't',	 'u',	 'v',	 'w',
	 'x',	 'y',	 'z',	0133,	0134,	0135,	0136,	0137,
	0140,	 'a',	 'b',	 'c',	 'd',	 'e',	 'f',	 'g',
	 'h',	 'i',	 'j',	 'k',	 'l',	 'm',	 'n',	 'o',
	 'p',	 'q',	 'r',	 's',	 't',	 'u',	 'v',	 'w',
	 'x',	 'y',	 'z',	0173,	0174,	0175,	0176,	0177,
};

char __mapupper[1 + 256] = {
	0,
	0000,	0001,	0002,	0003,	0004,	0005,	0006,	0007,
	0010,	0011,	0012,	0013,	0014,	0015,	0016,	0017,
	0020,	0021,	0022,	0023,	0024,	0025,	0026,	0027,
	0030,	0031,	0032,	0033,	0034,	0035,	0036,	0037,
	0040,	0041,	0042,	0043,	0044,	0045,	0046,	0047,
	0050,	0051,	0052,	0053,	0054,	0055,	0056,	0057,
	0060,	0061,	0062,	0063,	0064,	0065,	0066,	0067,
	0070,	0071,	0072,	0073,	0074,	0075,	0076,	0077,
	0100,	 'A',	 'B',	 'C',	 'D',	 'E',	 'F',	 'G',
	 'H',	 'I',	 'J',	 'K',	 'L',	 'M',	 'N',	 'O',
	 'P',	 'Q',	 'R',	 'S',	 'T',	 'U',	 'V',	 'W',
	 'X',	 'Y',	 'Z',	0133,	0134,	0135,	0136,	0137,
	0140,	 'A',	 'B',	 'C',	 'D',	 'E',	 'F',	 'G',
	 'H',	 'I',	 'J',	 'K',	 'L',	 'M',	 'N',	 'O',
	 'P',	 'Q',	 'R',	 'S',	 'T',	 'U',	 'V',	 'W',
	 'X',	 'Y',	 'Z',	0173,	0174,	0175,	0176,	0177,
};
