/* ==== fd_kern.c ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Deals with the valid kernel fds.
 *
 *  1.00 93/09/27 proven
 *      -Started coding this file.
 *
 *	1.01 93/11/13 proven
 *		-The functions readv() and writev() added.
 */

/*
 * Copyright (c) 1993 by Chris Provenzano and contributors, proven@mit.edu
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Chris Provenzano,
 *	the University of California, Berkeley, and contributors.
 * 4. Neither the name of Chris Provenzano, the University, nor the names of
 *	  contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO, THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread/posix.h>

/* ==========================================================================
 * fd_kern_poll()
 *
 * Called only from context_switch(). The kernel must be locked
 * and interrupts must be turned of.
 *
 * This function uses a linked list of waiting pthreads, NOT a queue.
 */ 
static struct pthread *fd_wait_read, *fd_wait_write;
static semaphore fd_wait_lock = SEMAPHORE_CLEAR;
static fd_set fd_set_read, fd_set_write;
static struct timeval zerotime = { 0, 0 };

void fd_kern_poll(void)
{
	struct pthread **pthread;
	semaphore *lock;
	int count;

	/* If someone has the lock then they are in RUNNING state, just return */
	lock = &fd_wait_lock;
	if (SEMAPHORE_TEST_AND_SET(lock)) {
		return;
	}
	if (fd_wait_read || fd_wait_write) {
		for (pthread = &fd_wait_read; *pthread; pthread = &((*pthread)->next)) {
			FD_SET((*pthread)->fd, &fd_set_read);
		}
		for (pthread = &fd_wait_write; *pthread; pthread = &((*pthread)->next)) {
			FD_SET((*pthread)->fd, &fd_set_write);
		}

		while ((count = select(dtablesize, &fd_set_read, &fd_set_write,
		  NULL, &zerotime)) < OK) {
			if (count = -EINTR) {
				continue;
			}
			PANIC();
		}
	
		for (pthread = &fd_wait_read; count && *pthread; ) {
			if (FD_ISSET((*pthread)->fd, &fd_set_read)) {
				/* Get lock on thread */

				(*pthread)->state = PS_RUNNING;
				*pthread = (*pthread)->next;
				count--;
				continue;
			} 
			pthread = &((*pthread)->next);
		}
					
		for (pthread = &fd_wait_write; count && *pthread; ) {
			if (FD_ISSET((*pthread)->fd, &fd_set_write)) {
				semaphore *plock;

				/* Get lock on thread */
				plock = &(*pthread)->lock;
				if (!(SEMAPHORE_TEST_AND_SET(plock))) {
					/* Thread locked, skip it. */
					(*pthread)->state = PS_RUNNING;
					*pthread = (*pthread)->next;
					SEMAPHORE_RESET(plock);
				}
				count--;
				continue;
			} 
			pthread = &((*pthread)->next);
		}
	}
	SEMAPHORE_RESET(lock);
}

/* ==========================================================================
 * Special Note: All operations return the errno as a negative of the errno
 * listed in errno.h
 * ======================================================================= */

/* ==========================================================================
 * read()
 */
ssize_t __fd_kern_read(int fd, int flags, void *buf, size_t nbytes)
{
	semaphore *lock, *plock;
	int ret;

	while ((ret = machdep_sys_read(fd, buf, nbytes)) < OK) { 
		if (ret == -EWOULDBLOCK) {
			/* Lock queue */
			lock = &fd_wait_lock;
			while (SEMAPHORE_TEST_AND_SET(lock)) {
				pthread_yield();
			}

			/* Lock pthread */
			plock = &(pthread_run->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				pthread_yield();
			}

			/* queue pthread for a FDR_WAIT */
			pthread_run->next = fd_wait_read;
			fd_wait_read = pthread_run;
			pthread_run->fd = fd;
			SEMAPHORE_RESET(lock);
			reschedule(PS_FDR_WAIT);
		} else {
			pthread_run->error = -ret;
			ret = NOTOK;
			break;
		}
	}
	return(ret);
}

/* ==========================================================================
 * readv()
 */
int __fd_kern_readv(int fd, int flags, struct iovec *iov, int iovcnt)
{
	semaphore *lock, *plock;
	int ret;

	while ((ret = machdep_sys_readv(fd, iov, iovcnt)) < OK) { 
		if (ret == -EWOULDBLOCK) {
			/* Lock queue */
			lock = &fd_wait_lock;
			while (SEMAPHORE_TEST_AND_SET(lock)) {
				pthread_yield();
			}

			/* Lock pthread */
			plock = &(pthread_run->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				pthread_yield();
			}

			/* queue pthread for a FDR_WAIT */
			pthread_run->next = fd_wait_read;
			fd_wait_read = pthread_run;
			pthread_run->fd = fd;
			SEMAPHORE_RESET(lock);
			reschedule(PS_FDR_WAIT);
		} else {
			pthread_run->error = -ret;
			ret = NOTOK;
			break;
		}
	}
	return(ret);
}

/* ==========================================================================
 * write()
 */
ssize_t __fd_kern_write(int fd, int flags, const void *buf, size_t nbytes)
{
	semaphore *lock, *plock;
	int ret;

    while ((ret = machdep_sys_write(fd, buf, nbytes)) < OK) { 
        if (pthread_run->error == -EWOULDBLOCK) {
			/* Lock queue */
			lock = &fd_wait_lock;
			while (SEMAPHORE_TEST_AND_SET(lock)) {
				pthread_yield();
			}

			/* Lock pthread */
			plock = &(pthread_run->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				pthread_yield();
			}

			/* queue pthread for a FDW_WAIT */
			pthread_run->next = fd_wait_write;
			fd_wait_write = pthread_run;
			pthread_run->fd = fd;
			SEMAPHORE_RESET(lock);
			reschedule(PS_FDW_WAIT);
        } else {
			pthread_run->error = ret;
            break;
        }
    }
    return(ret);
}

/* ==========================================================================
 * writev()
 */
int __fd_kern_writev(int fd, int flags, struct iovec *iov, int iovcnt)
{
	semaphore *lock, *plock;
	int ret;

    while ((ret = machdep_sys_writev(fd, iov, iovcnt)) < OK) { 
        if (pthread_run->error == -EWOULDBLOCK) {
			/* Lock queue */
			lock = &fd_wait_lock;
			while (SEMAPHORE_TEST_AND_SET(lock)) {
				pthread_yield();
			}

			/* Lock pthread */
			plock = &(pthread_run->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				pthread_yield();
			}

			/* queue pthread for a FDW_WAIT */
			pthread_run->next = fd_wait_write;
			fd_wait_write = pthread_run;
			pthread_run->fd = fd;
			SEMAPHORE_RESET(lock);
			reschedule(PS_FDW_WAIT);
        } else {
			pthread_run->error = ret;
            break;
        }
    }
    return(ret);
}

/* ==========================================================================
 * For blocking version we really should set an interrupt
 * fcntl()
 */
int __fd_kern_fcntl(int fd, int flags, int cmd, int arg)
{
	machdep_sys_fcntl(fd, cmd, arg);
}

/* ==========================================================================
 * close()
 */
int __fd_kern_close(int fd, int flags)
{
	machdep_sys_close(fd);
}

/*
 * File descriptor operations
 */
extern machdep_sys_close();

/* Normal file operations */
static struct fd_ops __fd_kern_ops = {
	__fd_kern_write, __fd_kern_read, __fd_kern_close, __fd_kern_fcntl,
	__fd_kern_readv, __fd_kern_writev
};

/* NFS file opperations */

/* FIFO file opperations */

/* Device operations */

/* ==========================================================================
 * open()
 *
 * Because open could potentially block opening a file from a remote
 * system, we want to make sure the call will timeout. We then try and open
 * the file, and stat the file to determine what operations we should
 * associate with the fd.
 *
 * This is not done yet
 *
 * A reqular file on the local system needs no special treatment.
 */
int open(const char *path, int flags, ...)
{
	int fd, mode, fd_kern;
	struct stat stat_buf;
	va_list ap;

	/* If pthread scheduling == FIFO set a virtual timer */
	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		mode = 0;
	}

	if (!((fd = fd_allocate()) < OK)) {
		fd_table[fd]->flags = flags;
		flags |= __FD_NONBLOCK;

		if (!((fd_kern = machdep_sys_open(path, flags, mode)) < OK)) {

			/* fstat the file to determine what type it is */
			if (fstat(fd_kern, &stat_buf)) {
printf("error %d stating new fd %d\n", errno, fd);
			}
			if (S_ISREG(stat_buf.st_mode)) {
				fd_table[fd]->ops = &(__fd_kern_ops);
				fd_table[fd]->type = FD_HALF_DUPLEX;
			} else {
				fd_table[fd]->ops = &(__fd_kern_ops);
				fd_table[fd]->type = FD_FULL_DUPLEX;
			}
			fd_table[fd]->fd.i = fd_kern; 
			return(fd);
		}

		pthread_run->error = - fd_kern;
		fd_table[fd]->count = 0;
	}
	return(NOTOK);
}

/* ==========================================================================
 * fd_kern_init()
 *
 * Assume the entry is locked before routine is invoked
 *
 * This may change. The problem is setting the fd to nonblocking changes
 * the parents fd too, which may not be the desired result.
 */
static fd_kern_init_called = 0;
void fd_kern_init(int fd)
{
	if ((fd_table[fd]->flags = machdep_sys_fcntl(fd, F_GETFL, NULL)) >= OK) {
		machdep_sys_fcntl(fd, F_SETFL, fd_table[fd]->flags | __FD_NONBLOCK);
		fd_table[fd]->ops 	= &(__fd_kern_ops);
		fd_table[fd]->type 	= FD_HALF_DUPLEX;
		fd_table[fd]->fd.i 	= fd;
		fd_table[fd]->count = 1;

		/* Only give one warning */
		if (!(fd_kern_init_called++)) {
			printf("Warning: threaded process may have changed open file ");
		    printf("descriptors of parent\n");
		}
	}
}

/* ==========================================================================
 * Here are the berkeley socket functions. These are not POSIX.
 * ======================================================================= */

/* ==========================================================================
 * socket()
 */
int socket(int af, int type, int protocol)
{
	int fd, fd_kern;

	 if (!((fd = fd_allocate()) < OK)) {

        if (!((fd_kern = machdep_sys_socket(af, type, protocol)) < OK)) {
			machdep_sys_fcntl(fd_kern, F_SETFL, __FD_NONBLOCK);

            /* Should fstat the file to determine what type it is */
            fd_table[fd]->ops 	= & __fd_kern_ops;
            fd_table[fd]->type 	= FD_FULL_DUPLEX;
			fd_table[fd]->fd.i	= fd_kern;
        	fd_table[fd]->flags = 0;
            return(fd);
        }

        pthread_run->error = - fd_kern;
        fd_table[fd]->count = 0;
    }
    return(NOTOK);
}

/* ==========================================================================
 * bind()
 */
int bind(int fd, const struct sockaddr *name, int namelen)
{
	/* Not much to do in bind */
	semaphore *plock;
	int ret;

	if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
        if ((ret = machdep_sys_bind(fd_table[fd]->fd, name, namelen)) < OK) { 
			pthread_run->error = - ret;
		}
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}

/* ==========================================================================
 * connect()
 */
int connect(int fd, const struct sockaddr *name, int namelen)
{
	semaphore *lock, *plock;
	struct sockaddr tmpname;
	int ret, tmpnamelen;

	if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
		if ((ret = machdep_sys_connect(fd_table[fd]->fd, name, namelen)) < OK) {
            if ((ret == -EWOULDBLOCK) || (ret == -EINPROGRESS) ||
		      (ret == -EALREADY)) {
				/* Lock queue */
				lock = &fd_wait_lock;
				while (SEMAPHORE_TEST_AND_SET(lock)) {
					pthread_yield();
				}

				/* Lock pthread */
				plock = &(pthread_run->lock);
				while (SEMAPHORE_TEST_AND_SET(plock)) {
					pthread_yield();
				}

				/* queue pthread for a FDW_WAIT */
				pthread_run->next = fd_wait_write;
				fd_wait_write = pthread_run;
				pthread_run->fd = fd;
				SEMAPHORE_RESET(lock);
				reschedule(PS_FDW_WAIT);

				/* OK now lets see if it really worked */
				if (((ret = machdep_sys_getpeername(fd_table[fd]->fd,
				  &tmpname, &tmpnamelen)) < OK) && (ret == -ENOTCONN)) {

					/* Get the error, this function should not fail */
					machdep_sys_getsockopt(fd_table[fd]->fd, SOL_SOCKET,
					  SO_ERROR, &pthread_run->error, &tmpnamelen); 
				}
            } else {
				pthread_run->error = -ret;
			}
		}
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}

/* ==========================================================================
 * accept()
 */
int accept(int fd, struct sockaddr *name, int *namelen)
{
	semaphore *lock, *plock;
	int ret;

	if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
		while ((ret = machdep_sys_accept(fd_table[fd]->fd, name, namelen)) < OK) {
            if (pthread_run->error == -EWOULDBLOCK) {
				/* Lock queue */
				lock = &fd_wait_lock;
				while (SEMAPHORE_TEST_AND_SET(lock)) {
					pthread_yield();
				}

				/* Lock pthread */
				plock = &(pthread_run->lock);
				while (SEMAPHORE_TEST_AND_SET(plock)) {
					pthread_yield();
				}

				/* queue pthread for a FDR_WAIT */
				pthread_run->next = fd_wait_read;
				fd_wait_read = pthread_run;
				pthread_run->fd = fd;
				SEMAPHORE_RESET(lock);
				reschedule(PS_FDR_WAIT);
            } else {
                break;
			}
		}
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}

/* ==========================================================================
 * listen()
 */
int listen(int fd, int backlog) 
{
	int ret;

	if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
		ret = machdep_sys_listen(fd_table[fd]->fd, backlog);
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}
