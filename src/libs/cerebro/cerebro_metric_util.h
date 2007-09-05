/*****************************************************************************\
 *  $Id: cerebro_metric_util.h,v 1.13 2007-09-05 18:15:56 chu11 Exp $
 *****************************************************************************
 *  Copyright (C) 2005 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Albert Chu <chu11@llnl.gov>.
 *  UCRL-CODE-155989 All rights reserved.
 *
 *  This file is part of Cerebro, a collection of cluster monitoring
 *  tools and libraries.  For details, see
 *  <http://www.llnl.gov/linux/cerebro/>.
 *
 *  Cerebro is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Cerebro is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Genders; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
\*****************************************************************************/

#ifndef _CEREBRO_METRIC_UTIL_H
#define _CEREBRO_METRIC_UTIL_H

#include "cerebro.h"

#include "cerebro/cerebro_metric_server_protocol.h"

/* 
 * Cerebro_metric_receive_response
 *
 * Function to call after the metric request has been sent
 */
typedef int (*Cerebro_metric_receive_response)(cerebro_t handle,
                                               void *list,
                                               struct cerebro_metric_server_response *res,
                                               unsigned int bytes_read,
                                               int fd);

/* 
 * _cerebro_metric_get_data
 *
 * Connect to the cerebrod metric and receive responses
 *
 * Returns 0 on success, -1 on error
 */
int _cerebro_metric_get_data(cerebro_t handle,
                             void *list,
                             const char *metric_name,
                             Cerebro_metric_receive_response receive_response);

#endif /* _CEREBRO_METRIC_UTIL_H */
