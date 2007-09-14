/*****************************************************************************
 *  $Id: fd.c,v 1.3 2007-09-14 23:40:56 chu11 Exp $
 *****************************************************************************
 *  Copyright (C) 2001-2002 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Chris Dunlap <cdunlap@llnl.gov>.
 *  UCRL-CODE-2002-009.
 *  
 *  This file is part of ConMan, a remote console management program.
 *  For details, see <http://www.llnl.gov/linux/conman/>.
 *  
 *  ConMan is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  ConMan is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with ConMan; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *****************************************************************************
 *  Refer to "fd.h" for documentation on public functions.
 *****************************************************************************/


#if HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */
#include <assert.h>
#include <errno.h>

#include "fd.h"


/*****************************************************************************
 *  Static Prototypes
 *****************************************************************************/

static int _fd_get_lock (int fd, int cmd, int type);
static pid_t _fd_test_lock (int fd, int type);


/*****************************************************************************
 *  Extern I/O Functions
 *****************************************************************************/

ssize_t
fd_read_n (int fd, void *buf, size_t n)
{
    size_t nleft;
    ssize_t nread;
    unsigned char *p;

    p = buf;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read (fd, p, nleft)) < 0) {
            if (errno == EINTR)
                continue;
            else
                return (-1);
        }
        else if (nread == 0) {          /* EOF */
            break;
        }
        nleft -= nread;
        p += nread;
    }
    return (n - nleft);
}


ssize_t
fd_write_n (int fd, const void *buf, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    unsigned char *p;

    p = (unsigned char *)buf;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write (fd, p, nleft)) < 0) {
            if (errno == EINTR)
                continue;
            else
                return (-1);
        }
        nleft -= nwritten;
        p += nwritten;
    }
    return (n);
}


ssize_t
fd_read_line (int fd, void *buf, size_t maxlen)
{
    ssize_t n, rc;
    unsigned char c, *p;

    n = 0;
    p = buf;
    while (n < maxlen - 1) {            /* reserve space for NUL-termination */

        if ((rc = read (fd, &c, 1)) == 1) {
            n++;
            *p++ = c;
            if (c == '\n')
                break;                  /* store newline, like fgets() */
        }
        else if (rc == 0) {
            if (n == 0)                 /* EOF, no data read */
                return (0);
            else                        /* EOF, some data read */
                break;
        }
        else {
            if (errno == EINTR)
                continue;
            return (-1);
        }
    }

    *p = '\0';                          /* NUL-terminate, like fgets() */
    return (n);
}


/*****************************************************************************
 *  Extern Locking Functions
 *****************************************************************************/

int
fd_get_read_lock (int fd)
{
    return (_fd_get_lock (fd, F_SETLK, F_RDLCK));
}


int
fd_get_readw_lock (int fd)
{
    return (_fd_get_lock (fd, F_SETLKW, F_RDLCK));
}


int
fd_get_write_lock (int fd)
{
    return (_fd_get_lock (fd, F_SETLK, F_WRLCK));
}


int
fd_get_writew_lock (int fd)
{
    return (_fd_get_lock (fd, F_SETLKW, F_WRLCK));
}


int
fd_release_lock (int fd)
{
    return (_fd_get_lock (fd, F_SETLK, F_UNLCK));
}


pid_t
fd_is_read_lock_blocked (int fd)
{
    return (_fd_test_lock (fd, F_RDLCK));
}


pid_t
fd_is_write_lock_blocked (int fd)
{
    return (_fd_test_lock (fd, F_WRLCK));
}


/*****************************************************************************
 *  Extern Attribute Functions
 *****************************************************************************/

int
fd_set_close_on_exec (int fd)
{
    assert (fd >= 0);

    if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0)
        return (-1);
    return (0);
}


int
fd_set_nonblocking (int fd)
{
    int fval;

    assert (fd >= 0);

    if ((fval = fcntl (fd, F_GETFL, 0)) < 0)
        return (-1);
    if (fcntl (fd, F_SETFL, fval | O_NONBLOCK) < 0)
        return (-1);
    return (0);
}


/*****************************************************************************
 *  Static Functions
 *****************************************************************************/

static int
_fd_get_lock (int fd, int cmd, int type)
{
    struct flock lock;

    assert (fd >= 0);

    lock.l_type = type;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    return (fcntl (fd, cmd, &lock));
}


static pid_t
_fd_test_lock (int fd, int type)
{
    struct flock lock;

    assert (fd >= 0);

    lock.l_type = type;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    if (fcntl (fd, F_GETLK, &lock) < 0)
        return (-1);
    if (lock.l_type == F_UNLCK)
        return (0);
    return (lock.l_pid);
}
