/* crypto/rand/md_rand.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
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
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#ifdef MD_RAND_DEBUG
# ifndef NDEBUG
#   define NDEBUG
# endif
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "openssl/e_os.h"

#include <openssl/rand.h>
#include "rand_lcl.h"

#include <openssl/crypto.h>
#include <openssl/err.h>

#ifdef BN_DEBUG
# define PREDICT
#endif

/* #define PREDICT	1 */

#define STATE_SIZE	1023
static int state_num=0,state_index=0;
static unsigned char state[STATE_SIZE+MD_DIGEST_LENGTH];
static unsigned char md[MD_DIGEST_LENGTH];
static long md_count[2]={0,0};
static double entropy=0;
static int initialized=0;

/* This should be set to 1 only when ssleay_rand_add() is called inside
   an already locked state, so it doesn't try to lock and thereby cause
   a hang.  And it should always be reset back to 0 before unlocking. */
static int add_do_not_lock=0;

#ifdef PREDICT
int rand_predictable=0;
#endif

const char *RAND_version="RAND" OPENSSL_VERSION_PTEXT;

static void ssleay_rand_cleanup(void);
static void ssleay_rand_seed(const void *buf, int num);
static void ssleay_rand_add(const void *buf, int num, double add_entropy);
static int ssleay_rand_bytes(unsigned char *buf, int num);
static int ssleay_rand_pseudo_bytes(unsigned char *buf, int num);
static int ssleay_rand_status(void);

RAND_METHOD rand_ssleay_meth={
	ssleay_rand_seed,
	ssleay_rand_bytes,
	ssleay_rand_cleanup,
	ssleay_rand_add,
	ssleay_rand_pseudo_bytes,
	ssleay_rand_status
	}; 

RAND_METHOD *RAND_SSLeay(void)
	{
	return(&rand_ssleay_meth);
	}

static void ssleay_rand_cleanup(void)
	{
	memset(state,0,sizeof(state));
	state_num=0;
	state_index=0;
	memset(md,0,MD_DIGEST_LENGTH);
	md_count[0]=0;
	md_count[1]=0;
	entropy=0;
	initialized=0;
	}

static void ssleay_rand_add(const void *buf, int num, double add)
	{
	int i,j,k,st_idx;
	long md_c[2];
	unsigned char local_md[MD_DIGEST_LENGTH];
	MD_CTX m;

	/*
	 * (Based on the rand(3) manpage)
	 *
	 * The input is chopped up into units of 20 bytes (or less for
	 * the last block).  Each of these blocks is run through the hash
	 * function as follows:  The data passed to the hash function
	 * is the current 'md', the same number of bytes from the 'state'
	 * (the location determined by in incremented looping index) as
	 * the current 'block', the new key data 'block', and 'count'
	 * (which is incremented after each use).
	 * The result of this is kept in 'md' and also xored into the
	 * 'state' at the same locations that were used as input into the
         * hash function.
	 */

	if (!add_do_not_lock) CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	st_idx=state_index;

	/* use our own copies of the counters so that even
	 * if a concurrent thread seeds with exactly the
	 * same data and uses the same subarray there's _some_
	 * difference */
	md_c[0] = md_count[0];
	md_c[1] = md_count[1];

	memcpy(local_md, md, sizeof md);

	/* state_index <= state_num <= STATE_SIZE */
	state_index += num;
	if (state_index >= STATE_SIZE)
		{
		state_index%=STATE_SIZE;
		state_num=STATE_SIZE;
		}
	else if (state_num < STATE_SIZE)	
		{
		if (state_index > state_num)
			state_num=state_index;
		}
	/* state_index <= state_num <= STATE_SIZE */

	/* state[st_idx], ..., state[(st_idx + num - 1) % STATE_SIZE]
	 * are what we will use now, but other threads may use them
	 * as well */

	md_count[1] += (num / MD_DIGEST_LENGTH) + (num % MD_DIGEST_LENGTH > 0);

	if (!add_do_not_lock) CRYPTO_w_unlock(CRYPTO_LOCK_RAND);

	for (i=0; i<num; i+=MD_DIGEST_LENGTH)
		{
		j=(num-i);
		j=(j > MD_DIGEST_LENGTH)?MD_DIGEST_LENGTH:j;

		MD_Init(&m);
		MD_Update(&m,local_md,MD_DIGEST_LENGTH);
		k=(st_idx+j)-STATE_SIZE;
		if (k > 0)
			{
			MD_Update(&m,&(state[st_idx]),j-k);
			MD_Update(&m,&(state[0]),k);
			}
		else
			MD_Update(&m,&(state[st_idx]),j);
			
		MD_Update(&m,buf,j);
		MD_Update(&m,(unsigned char *)&(md_c[0]),sizeof(md_c));
		MD_Final(local_md,&m);
		md_c[1]++;

		buf=(const char *)buf + j;

		for (k=0; k<j; k++)
			{
			/* Parallel threads may interfere with this,
			 * but always each byte of the new state is
			 * the XOR of some previous value of its
			 * and local_md (itermediate values may be lost).
			 * Alway using locking could hurt performance more
			 * than necessary given that conflicts occur only
			 * when the total seeding is longer than the random
			 * state. */
			state[st_idx++]^=local_md[k];
			if (st_idx >= STATE_SIZE)
				st_idx=0;
			}
		}
	memset((char *)&m,0,sizeof(m));

	if (!add_do_not_lock) CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	/* Don't just copy back local_md into md -- this could mean that
	 * other thread's seeding remains without effect (except for
	 * the incremented counter).  By XORing it we keep at least as
	 * much entropy as fits into md. */
	for (k = 0; k < sizeof md; k++)
		{
		md[k] ^= local_md[k];
		}
	if (entropy < ENTROPY_NEEDED) /* stop counting when we have enough */
	    entropy += add;
	if (!add_do_not_lock) CRYPTO_w_unlock(CRYPTO_LOCK_RAND);
	
#if !defined(THREADS) && !defined(WIN32)
	assert(md_c[1] == md_count[1]);
#endif
	}

static void ssleay_rand_seed(const void *buf, int num)
	{
	ssleay_rand_add(buf, num, num);
	}

static int ssleay_rand_bytes(unsigned char *buf, int num)
	{
	static volatile int stirred_pool = 0;
	int i,j,k,st_num,st_idx;
	int ok;
	long md_c[2];
	unsigned char local_md[MD_DIGEST_LENGTH];
	MD_CTX m;
#ifndef GETPID_IS_MEANINGLESS
	pid_t curr_pid = getpid();
#endif
	int do_stir_pool = 0;

#ifdef PREDICT
	if (rand_predictable)
		{
		static unsigned char val=0;

		for (i=0; i<num; i++)
			buf[i]=val++;
		return(1);
		}
#endif

	/*
	 * (Based on the rand(3) manpage:)
	 *
	 * For each group of 10 bytes (or less), we do the following:
	 *
	 * Input into the hash function the top 10 bytes from the
	 * local 'md' (which is initialized from the global 'md'
	 * before any bytes are generated), the bytes that are
	 * to be overwritten by the random bytes, and bytes from the
	 * 'state' (incrementing looping index).  From this digest output
	 * (which is kept in 'md'), the top (up to) 10 bytes are
	 * returned to the caller and the bottom (up to) 10 bytes are xored
	 * into the 'state'.
	 * Finally, after we have finished 'num' random bytes for the
	 * caller, 'count' (which is incremented) and the local and global 'md'
	 * are fed into the hash function and the results are kept in the
	 * global 'md'.
	 */

	if (!initialized)
		RAND_poll();

	CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	add_do_not_lock = 1;	/* Since we call ssleay_rand_add while in
				   this locked state. */

	initialized = 1;
	if (!stirred_pool)
		do_stir_pool = 1;
	
	ok = (entropy >= ENTROPY_NEEDED);
	if (!ok)
		{
		/* If the PRNG state is not yet unpredictable, then seeing
		 * the PRNG output may help attackers to determine the new
		 * state; thus we have to decrease the entropy estimate.
		 * Once we've had enough initial seeding we don't bother to
		 * adjust the entropy count, though, because we're not ambitious
		 * to provide *information-theoretic* randomness.
		 *
		 * NOTE: This approach fails if the program forks before
		 * we have enough entropy. Entropy should be collected
		 * in a separate input pool and be transferred to the
		 * output pool only when the entropy limit has been reached.
		 */
		entropy -= num;
		if (entropy < 0)
			entropy = 0;
		}

	if (do_stir_pool)
		{
		/* Our output function chains only half of 'md', so we better
		 * make sure that the required entropy gets 'evenly distributed'
		 * through 'state', our randomness pool.  The input function
		 * (ssleay_rand_add) chains all of 'md', which makes it more
		 * suitable for this purpose.
		 */

		int n = STATE_SIZE; /* so that the complete pool gets accessed */
		while (n > 0)
			{
#if MD_DIGEST_LENGTH > 20
# error "Please adjust DUMMY_SEED."
#endif
#define DUMMY_SEED "...................." /* at least MD_DIGEST_LENGTH */
			/* Note that the seed does not matter, it's just that
			 * ssleay_rand_add expects to have something to hash. */
			ssleay_rand_add(DUMMY_SEED, MD_DIGEST_LENGTH, 0.0);
			n -= MD_DIGEST_LENGTH;
			}
		if (ok)
			stirred_pool = 1;
		}

	st_idx=state_index;
	st_num=state_num;
	md_c[0] = md_count[0];
	md_c[1] = md_count[1];
	memcpy(local_md, md, sizeof md);

	state_index+=num;
	if (state_index > state_num)
		state_index %= state_num;

	/* state[st_idx], ..., state[(st_idx + num - 1) % st_num]
	 * are now ours (but other threads may use them too) */

	md_count[0] += 1;

	add_do_not_lock = 0;	/* If this would ever be forgotten, we can
				   expect any evil god to eat our souls. */
	CRYPTO_w_unlock(CRYPTO_LOCK_RAND);

	while (num > 0)
		{
		j=(num >= MD_DIGEST_LENGTH/2)?MD_DIGEST_LENGTH/2:num;
		num-=j;
		MD_Init(&m);
#ifndef GETPID_IS_MEANINGLESS
		if (curr_pid) /* just in the first iteration to save time */
			{
			MD_Update(&m,(unsigned char*)&curr_pid,sizeof curr_pid);
			curr_pid = 0;
			}
#endif
		MD_Update(&m,&(local_md[MD_DIGEST_LENGTH/2]),MD_DIGEST_LENGTH/2);
		MD_Update(&m,(unsigned char *)&(md_c[0]),sizeof(md_c));
#ifndef PURIFY
		MD_Update(&m,buf,j); /* purify complains */
#endif
		k=(st_idx+j)-st_num;
		if (k > 0)
			{
			MD_Update(&m,&(state[st_idx]),j-k);
			MD_Update(&m,&(state[0]),k);
			}
		else
			MD_Update(&m,&(state[st_idx]),j);
		MD_Final(local_md,&m);

		for (i=0; i<j; i++)
			{
			state[st_idx++]^=local_md[i]; /* may compete with other threads */
			*(buf++)=local_md[i+MD_DIGEST_LENGTH/2];
			if (st_idx >= st_num)
				st_idx=0;
			}
		}

	MD_Init(&m);
	MD_Update(&m,(unsigned char *)&(md_c[0]),sizeof(md_c));
	MD_Update(&m,local_md,MD_DIGEST_LENGTH);
	CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	MD_Update(&m,md,MD_DIGEST_LENGTH);
	MD_Final(md,&m);
	CRYPTO_w_unlock(CRYPTO_LOCK_RAND);

	memset(&m,0,sizeof(m));
	if (ok)
		return(1);
	else
		{
		RANDerr(RAND_F_SSLEAY_RAND_BYTES,RAND_R_PRNG_NOT_SEEDED);
		ERR_add_error_data(1, "You need to read the OpenSSL FAQ, "
			"http://www.openssl.org/support/faq.html");
		return(0);
		}
	}

/* pseudo-random bytes that are guaranteed to be unique but not
   unpredictable */
static int ssleay_rand_pseudo_bytes(unsigned char *buf, int num) 
	{
	int ret, err;

	ret = RAND_bytes(buf, num);
	if (ret == 0)
		{
		err = ERR_peek_error();
		if (ERR_GET_LIB(err) == ERR_LIB_RAND &&
		    ERR_GET_REASON(err) == RAND_R_PRNG_NOT_SEEDED)
			(void)ERR_get_error();
		}
	return (ret);
	}

static int ssleay_rand_status(void)
	{
	int ret;

	if (!initialized)
		RAND_poll();

	CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	initialized = 1;
	ret = entropy >= ENTROPY_NEEDED;
	CRYPTO_w_unlock(CRYPTO_LOCK_RAND);

	return ret;
	}
