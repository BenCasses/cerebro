/*****************************************************************************\
 *  $Id: cerebro_metric_util.h,v 1.8 2005-07-01 20:14:23 achu Exp $
\*****************************************************************************/

#ifndef _CEREBRO_METRIC_UTIL_H
#define _CEREBRO_METRIC_UTIL_H

#include "cerebro.h"

/* 
 * Cerebro_metric_response_receive
 *
 * Function to call after the metric request has been sent
 */
typedef int (*Cerebro_metric_response_receive)(cerebro_t handle,
                                               void *list,
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
                             Cerebro_metric_response_receive response_receive);

/*
 * _cerebro_metric_response_check
 *
 * Check that the version and error code are good prior to unmarshalling
 *
 * Returns 0 on success, -1 on error
 */
int _cerebro_metric_response_check(cerebro_t handle, 
                                   const char *buf, 
                                   unsigned int buflen);


#endif /* _CEREBRO_METRIC_UTIL_H */
