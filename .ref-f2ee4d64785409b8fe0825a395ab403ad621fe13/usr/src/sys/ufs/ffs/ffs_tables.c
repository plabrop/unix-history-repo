/* Copyright (c) 1981 Regents of the University of California */

/* @(#)ffs_tables.c 1.3 %G% */

#include "../h/param.h"

/*	partab.c	4.2	81/03/08	*/

/*
 * Table giving parity for characters and indicating
 * character classes to tty driver.  In particular,
 * if the low 6 bits are 0, then the character needs
 * no special processing on output.
 */

unsigned char partab[] = {
	0001,0201,0201,0001,0201,0001,0001,0201,
	0202,0004,0003,0201,0005,0206,0201,0001,
	0201,0001,0001,0201,0001,0201,0201,0001,
	0001,0201,0201,0001,0201,0001,0001,0201,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0201,

	/*
	 * 7 bit ascii ends with the last character above,
	 * but we contine through all 256 codes for the sake
	 * of the tty output routines which use special vax
	 * instructions which need a 256 character trt table.
	 */

	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007,
	0007,0007,0007,0007,0007,0007,0007,0007
};

/*
 * bit patterns for identifying fragments in the block map
 * used as ((map & around) == inside)
 */
int around[9] = {
	0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff, 0x1ff, 0x3ff
};
int inside[9] = {
	0x0, 0x2, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe
};

/*
 * given a block map bit pattern, the frag tables tell whether a
 * particular size fragment is available. 
 *
 * used as:
 * if ((1 << (size - 1)) & fragtbl[fs->fs_frag][map] {
 *	at least one fragment of the indicated size is available
 * }
 *
 * These tables are used by the scanc instruction on the VAX to
 * quickly find an appropriate fragment.
 */

unsigned char fragtbl1[256] = {
	0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
};

unsigned char fragtbl2[256] = {
	0x0, 0x1, 0x1, 0x2, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x2, 0x3, 0x3, 0x2,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x2, 0x3, 0x3, 0x2, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x2, 0x3, 0x3, 0x2,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x3,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x2, 0x3, 0x3, 0x2, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x2, 0x3, 0x3, 0x2,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x2, 0x3, 0x3, 0x2, 0x3, 0x3, 0x3, 0x3,
	0x3, 0x3, 0x3, 0x3, 0x2, 0x3, 0x3, 0x2,
};

unsigned char fragtbl4[256] = {
	0x0, 0x1, 0x1, 0x2, 0x1, 0x1, 0x2, 0x4,
	0x1, 0x1, 0x1, 0x3, 0x2, 0x3, 0x4, 0x8,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x3, 0x5,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x5, 0x9,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x3, 0x5,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x5, 0x9,
	0x2, 0x3, 0x3, 0x2, 0x3, 0x3, 0x2, 0x6,
	0x3, 0x3, 0x3, 0x3, 0x2, 0x3, 0x6, 0xa,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x3, 0x5,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x5, 0x9,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x3, 0x5,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x5, 0x9,
	0x2, 0x3, 0x3, 0x2, 0x3, 0x3, 0x2, 0x6,
	0x3, 0x3, 0x3, 0x3, 0x2, 0x3, 0x6, 0xa,
	0x4, 0x5, 0x5, 0x6, 0x5, 0x5, 0x6, 0x4,
	0x5, 0x5, 0x5, 0x7, 0x6, 0x7, 0x4, 0xc,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x3, 0x5,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x5, 0x9,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x3, 0x5,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x5, 0x9,
	0x1, 0x1, 0x1, 0x3, 0x1, 0x1, 0x3, 0x5,
	0x1, 0x1, 0x1, 0x3, 0x3, 0x3, 0x5, 0x9,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x7,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x7, 0xb,
	0x2, 0x3, 0x3, 0x2, 0x3, 0x3, 0x2, 0x6,
	0x3, 0x3, 0x3, 0x3, 0x2, 0x3, 0x6, 0xa,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x7,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x7, 0xb,
	0x4, 0x5, 0x5, 0x6, 0x5, 0x5, 0x6, 0x4,
	0x5, 0x5, 0x5, 0x7, 0x6, 0x7, 0x4, 0xc,
	0x8, 0x9, 0x9, 0xa, 0x9, 0x9, 0xa, 0xc,
	0x9, 0x9, 0x9, 0xb, 0xa, 0xb, 0xc, 0x8,
};

unsigned char fragtbl8[256] = {
	0x00, 0x01, 0x01, 0x02, 0x01, 0x01, 0x02, 0x04,
	0x01, 0x01, 0x01, 0x03, 0x02, 0x03, 0x04, 0x08,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x02, 0x03, 0x03, 0x02, 0x04, 0x05, 0x08, 0x10,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x05, 0x09,
	0x02, 0x03, 0x03, 0x02, 0x03, 0x03, 0x02, 0x06,
	0x04, 0x05, 0x05, 0x06, 0x08, 0x09, 0x10, 0x20,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x05, 0x09,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x03, 0x03, 0x03, 0x03, 0x05, 0x05, 0x09, 0x11,
	0x02, 0x03, 0x03, 0x02, 0x03, 0x03, 0x02, 0x06,
	0x03, 0x03, 0x03, 0x03, 0x02, 0x03, 0x06, 0x0a,
	0x04, 0x05, 0x05, 0x06, 0x05, 0x05, 0x06, 0x04,
	0x08, 0x09, 0x09, 0x0a, 0x10, 0x11, 0x20, 0x40,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x05, 0x09,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x03, 0x03, 0x03, 0x03, 0x05, 0x05, 0x09, 0x11,
	0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x03, 0x05,
	0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x05, 0x09,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x07,
	0x05, 0x05, 0x05, 0x07, 0x09, 0x09, 0x11, 0x21,
	0x02, 0x03, 0x03, 0x02, 0x03, 0x03, 0x02, 0x06,
	0x03, 0x03, 0x03, 0x03, 0x02, 0x03, 0x06, 0x0a,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x07,
	0x02, 0x03, 0x03, 0x02, 0x06, 0x07, 0x0a, 0x12,
	0x04, 0x05, 0x05, 0x06, 0x05, 0x05, 0x06, 0x04,
	0x05, 0x05, 0x05, 0x07, 0x06, 0x07, 0x04, 0x0c,
	0x08, 0x09, 0x09, 0x0a, 0x09, 0x09, 0x0a, 0x0c,
	0x10, 0x11, 0x11, 0x12, 0x20, 0x21, 0x40, 0x80,
};

/*
 * the actual fragtbl array
 */
unsigned char *fragtbl[MAXFRAG + 1] = {
	0, fragtbl1, fragtbl2, 0, fragtbl4, 0, 0, 0, fragtbl8,
};
