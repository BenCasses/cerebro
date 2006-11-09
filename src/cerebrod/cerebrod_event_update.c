/*****************************************************************************\
 *  $Id: cerebrod_event_update.c,v 1.3 2006-11-09 23:20:08 chu11 Exp $
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
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#if !WITH_CEREBROD_SPEAKER_ONLY

#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#include <errno.h>
#include <assert.h>

#include "cerebro.h"
#include "cerebro/cerebro_constants.h"

#include "cerebrod_config.h"
#include "cerebrod_listener_data.h"
#include "cerebrod_event_update.h"
#include "cerebrod_event_server.h"
#include "cerebrod_util.h"

#include "debug.h"
#include "event_module.h"
#include "hash.h"
#include "list.h"
#include "wrappers.h"

#define EVENT_NODE_TIMEOUT_SIZE_DEFAULT         1024
#define EVENT_NODE_TIMEOUT_SIZE_INCREMENT       1024

extern struct cerebrod_config conf;
#if CEREBRO_DEBUG
extern pthread_mutex_t debug_output_mutex;
#endif /* CEREBRO_DEBUG */

extern pthread_mutex_t listener_data_init_lock;

extern hash_t listener_data;
extern int listener_data_numnodes;
extern int listener_data_size;
extern pthread_mutex_t listener_data_lock;

/*
 * event_handle
 *
 * Handle for event modules;
 */
event_modules_t event_handle = NULL;

/*
 * event_index
 *
 * hash indexes to quickly determine what metrics are needed
 * by which modules.
 */
hash_t event_index = NULL;

/* 
 * event_module_timeout_index
 *
 * Map of timeouts to modules that need to be called
 */
hash_t event_module_timeout_index = NULL;

/*
 * event_names
 *
 * List of event names supported by the modules
 */
List event_names = NULL;

/* 
 * event_module_timeouts
 *
 * List of module timeout lengths
 */
List event_module_timeouts = NULL;
unsigned int event_module_timeout_min = INT_MAX;

/* 
 * event_node_timeout_data
 * event_node_timeout_data_index
 *
 * data for calculating when nodes timeout and informing the
 * appropriate modules.
 */
List event_node_timeout_data = NULL;
hash_t event_node_timeout_data_index = NULL;
int event_node_timeout_data_index_numnodes;
int event_node_timeout_data_index_size;

/*  
 * event_node_timeout_data_lock
 *
 * lock to protect pthread access to both the event_node_timeout_data
 * and event_node_timeout_data_index
 */
pthread_mutex_t event_node_timeout_data_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * event_queue
 * event_queue_cond
 * event_queue_lock
 *
 * queue of events to send out and the conditional variable and mutex
 * for exclusive access and signaling.
 */
List event_queue = NULL;
pthread_cond_t event_queue_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t event_queue_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * _cerebrod_event_module_info_destroy
 *
 * Destroy a event_module_info struct.
 */
static void
_cerebrod_event_module_info_destroy(void *data)
{
  struct cerebrod_event_module_info *event_module;

  assert(data);

  event_module = (struct cerebrod_event_module_info *)data;
  Free(event_module->metric_names);
  Free(event_module);
}

/* 
 * _cerebrod_event_module_timeout_data_destroy
 *
 * Destroy a monitor_module struct.
 */
static void
_cerebrod_event_module_timeout_data_destroy(void *data)
{
  struct cerebrod_event_module_timeout_data *mtd;
  
  assert(data);
  
  mtd = (struct cerebrod_event_module_timeout_data *)data;
  Free(mtd->timeout_str);
  Free(mtd);
}

/* 
 * _event_node_timeout_data_add
 *
 * Create entries for the event_node_timeout_data list and index
 *
 * Returns 0 on success, -1 on error
 */
static int
_event_node_timeout_data_add(const char *nodename, u_int32_t time_now)
{
  struct cerebrod_event_node_timeout_data *ntd;

  ntd = (struct cerebrod_event_node_timeout_data *)Malloc(sizeof(struct cerebrod_event_node_timeout_data));
  ntd->nodename = (char *)nodename;
  ntd->last_received_time = time_now;
  ntd->timeout_occurred = 0;

  List_append(event_node_timeout_data, ntd);
  Hash_insert(event_node_timeout_data_index, ntd->nodename, ntd);
  event_node_timeout_data_index_numnodes++;
  return 0;
}

/* 
 * _event_node_timeout_data_callback
 *
 * Callback from Hash_for_each on listener_data to create entries for
 * the event_node_timeout_data list and index for the first time.
 *
 * Returns 1 on success, 0 if not, -1 on error
 */
static int
_event_node_timeout_data_callback(void *data, const void *key, void *arg)
{
  struct cerebrod_node_data *nd;
  u_int32_t *time_now = (u_int32_t *)arg;

  nd = (struct cerebrod_node_data *)data;

  _event_node_timeout_data_add(nd->nodename, *time_now);
  return 1;
}

/* 
 * _event_module_timeout_data_find_callback
 *
 * Callback for list_find to find matching timeout
 */
static int
_event_module_timeout_data_find_callback(void *x, void *key)
{
  struct cerebrod_event_module_timeout_data *mtd;
  int timeout;

  assert(x);
  assert(key);

  timeout = *((int *)key);
  mtd = (struct cerebrod_event_module_timeout_data *)x;
  
  if (mtd->timeout == timeout)
    return 1;
  return 0;
}

/*
 * _event_module_timeout_data_compare
 *
 * Callback from list_sort for comparsion of data in
 * cerebrod_event_module_timeout_data.
 */
static int
_event_module_timeout_data_compare(void *x, void *y)
{
  struct cerebrod_event_module_timeout_data *a;
  struct cerebrod_event_module_timeout_data *b;

  a = (struct cerebrod_event_module_timeout_data *)x;
  b = (struct cerebrod_event_module_timeout_data *)y;

  if (a->timeout < b->timeout)
    return -1;
  if (a->timeout > b->timeout)
    return 1;
  return 0;
}

/*
 * _setup_event_node_timeout_data
 *
 * Setup event timeout calculation data structures. 
 *
 * Return 0 on success, -1 on error
 */
static int
_setup_event_node_timeout_data(void)
{
  struct timeval tv;
  int num;

  assert(conf.event_server);
  assert(listener_data);

  event_node_timeout_data = List_create(NULL);
  event_node_timeout_data_index = Hash_create(EVENT_NODE_TIMEOUT_SIZE_DEFAULT, 
                                              (hash_key_f)hash_key_string,
                                              (hash_cmp_f)strcmp, 
                                              (hash_del_f)NULL);
  event_node_timeout_data_index_size = EVENT_NODE_TIMEOUT_SIZE_DEFAULT;
  
  Gettimeofday(&tv, NULL);

  num = Hash_for_each(listener_data, 
                      _event_node_timeout_data_callback,
                      &(tv.tv_sec));
  if (num != listener_data_numnodes)
    {
      fprintf(stderr, "* invalid create count: num=%d numnodes=%d\n",
              num, listener_data_numnodes);
      goto cleanup;
    }

  return 0;

 cleanup:
  if (event_node_timeout_data)
    {
      List_destroy(event_node_timeout_data);
      event_node_timeout_data = NULL;
    }
  if (event_node_timeout_data_index)
    {
      Hash_destroy(event_node_timeout_data_index);
      event_node_timeout_data_index =  NULL;
    }
  return -1;
}

/* 
 * _cerebrod_event_to_send_destroy
 */
static void
_cerebrod_event_to_send_destroy(void *x)
{
  struct cerebrod_event_to_send *ets;

  assert(x);
  assert(event_handle);

  ets = (struct cerebrod_event_to_send *)x;
  event_module_destroy(event_handle, ets->index, ets->event);
  Free(ets);
}

/*
 * Under almost any circumstance, don't return a -1 error, cerebro can
 * go on without loading monitor modules. The listener_data_init_lock
 * should already be set.
 */
int
cerebrod_event_modules_setup(void)
{
  int i, event_module_count, event_index_len, event_index_count = 0;
  List event_list = NULL;
#if CEREBRO_DEBUG
  int rv;
#endif /* CEREBRO_DEBUG */

  assert(listener_data);

#if CEREBRO_DEBUG
  /* Should be called with lock already set */
  rv = Pthread_mutex_trylock(&listener_data_init_lock);
  if (rv != EBUSY)
    CEREBRO_EXIT(("mutex not locked: rv=%d", rv));
#endif /* CEREBRO_DEBUG */

  if (!conf.event_server)
    return 0;

  if (!(event_handle = event_modules_load()))
    {
      CEREBRO_DBG(("event_modules_load"));
      goto cleanup;
    }

  if ((event_module_count = event_modules_count(event_handle)) < 0)
    {
      CEREBRO_DBG(("event_modules_count"));
      goto cleanup;
    }

  if (!event_module_count)
    {
#if CEREBRO_DEBUG
      if (conf.debug && conf.event_server_debug)
        {
          Pthread_mutex_lock(&debug_output_mutex);
          fprintf(stderr, "**************************************\n");
          fprintf(stderr, "* No Event Modules Found\n");
          fprintf(stderr, "**************************************\n");
          Pthread_mutex_unlock(&debug_output_mutex);
        }
#endif /* CEREBRO_DEBUG */
      goto cleanup;
    }

  /* Each event module may want multiple metrics and/or offer multiple
   * event names.  We'll assume there will never be more than 2 per
   * event module, and that will be enough to avoid all hash
   * collisions.
   */
  event_index_len = event_module_count * 2;

  event_index = Hash_create(event_index_len,
                            (hash_key_f)hash_key_string,
                            (hash_cmp_f)strcmp,
                            (hash_del_f)list_destroy);

  event_names = List_create((ListDelF)_Free);

  event_module_timeouts = List_create((ListDelF)_cerebrod_event_module_timeout_data_destroy);
  event_module_timeout_index = Hash_create(event_module_count,
                                         (hash_key_f)hash_key_string,
                                         (hash_cmp_f)strcmp,
                                         (hash_del_f)list_destroy);

  for (i = 0; i < event_module_count; i++)
    {
      struct cerebrod_event_module *event_module;
      char *module_name, *module_metric_names, *module_event_names;
      char *metricPtr, *metricbuf;
      char *eventnamePtr, *eventbuf;
      int timeout;

      module_name = event_module_name(event_handle, i);

#if CEREBRO_DEBUG
      if (conf.debug && conf.event_server_debug)
        {
          Pthread_mutex_lock(&debug_output_mutex);
          fprintf(stderr, "**************************************\n");
          fprintf(stderr, "* Setup Event Module: %s\n", module_name);
          fprintf(stderr, "**************************************\n");
          Pthread_mutex_unlock(&debug_output_mutex);
        }
#endif /* CEREBRO_DEBUG */

      if (event_module_setup(event_handle, i) < 0)
        {
          CEREBRO_DBG(("event_module_setup failed: %s", module_name));
          continue;
        }

      if (!(module_metric_names = event_module_metric_names(event_handle, i)) < 0)
        {
          CEREBRO_DBG(("event_module_metric_names failed: %s", module_name));
          event_module_cleanup(event_handle, i);
          continue;
        }

      if (!(module_event_names = event_module_event_names(event_handle, i)) < 0)
        {
          CEREBRO_DBG(("event_module_event_names failed: %s", module_name));
          event_module_cleanup(event_handle, i);
          continue;
        }
      
      if ((timeout = event_module_timeout_length(event_handle, i)) < 0)
        {
          CEREBRO_DBG(("event_module_timeout_length failed: %s", module_name));
          event_module_cleanup(event_handle, i);
          continue;
        }

      event_module = Malloc(sizeof(struct cerebrod_event_module_info));
      event_module->metric_names = Strdup(module_metric_names);
      event_module->event_names = Strdup(module_event_names);
      event_module->index = i;
      Pthread_mutex_init(&(event_module->event_lock), NULL);

      /* The monitoring module may support multiple metrics */
          
      metricPtr = strtok_r(event_module->metric_names, ",", &metricbuf);
      while (metricPtr)
        {
          if (!(event_list = Hash_find(event_index, metricPtr)))
            {
              event_list = List_create((ListDelF)_cerebrod_event_module_info_destroy);
              List_append(event_list, event_module);
              Hash_insert(event_index, metricPtr, event_list);
              event_index_count++;
            }
          else
            List_append(event_list, event_module);
          
          metricPtr = strtok_r(NULL, ",", &metricbuf);
        }

      /* The monitoring module may support multiple event names */

      eventnamePtr = strtok_r(event_module->event_names, ",", &eventbuf);
      while (eventnamePtr)
        {
          if (!list_find_first(event_names,
                               (ListFindF)strcmp,
                               eventnamePtr))
            {
              List_append(event_names, eventnamePtr);
#if CEREBRO_DEBUG
              if (conf.debug && conf.event_server_debug)
                {
                  Pthread_mutex_lock(&debug_output_mutex);
                  fprintf(stderr, "**************************************\n");
                  fprintf(stderr, "* Event Name: %s\n", eventnamePtr);
                  fprintf(stderr, "**************************************\n");
                  Pthread_mutex_unlock(&debug_output_mutex);
                }
#endif /* CEREBRO_DEBUG */
            }
          eventnamePtr = strtok_r(NULL, ",", &eventbuf);
        }

      if (timeout)
        {
          struct cerebrod_event_module_timeout_data *mtd;
          List modules_list;

          if (!(mtd = List_find_first(event_module_timeouts, 
                                      _event_module_timeout_data_find_callback, 
                                      &timeout)))
            {
              char strbuf[64];

              mtd = (struct cerebrod_event_module_timeout_data *)Malloc(sizeof(struct cerebrod_event_module_timeout_data));
              mtd->timeout = timeout;
              snprintf(strbuf, 64, "%d", timeout);
              mtd->timeout_str = Strdup(strbuf);

              List_append(event_module_timeouts, mtd);
              
              if (timeout < event_module_timeout_min)
                event_module_timeout_min = timeout;
            }

          if (!(modules_list = Hash_find(event_module_timeout_index, 
                                         mtd->timeout_str)))
            {
              modules_list = List_create((ListDelF)NULL);
              List_append(modules_list, event_module);
              Hash_insert(event_module_timeout_index,
                          mtd->timeout_str,
                          modules_list);
            }
          else
            List_append(modules_list, event_module);
        }
    }
  
  List_sort(event_module_timeouts, _event_module_timeout_data_compare);

  if (!event_index_count)
    goto cleanup;

  if (_setup_event_node_timeout_data() < 0)
    goto cleanup;

  /* 
   * Since the cerebrod listener is started before any of the event
   * threads (node_timeout, queue_monitor, server), this must be
   * created in here (which is called by the listener) to avoid a
   * possible race of modules creating events before the event_queue
   * is created.
   */
  event_queue = List_create((ListDelF)_cerebrod_event_to_send_destroy);

  return 1;
  
 cleanup:
  if (event_handle)
    {
      event_modules_unload(event_handle);
      event_handle = NULL;
    }
  if (event_index)
    {
      Hash_destroy(event_index);
      event_index = NULL;
    }
  if (event_names)
    {
      List_destroy(event_names);
      event_names = NULL;
    }
  return 0;
}

void 
cerebrod_event_add_node_timeout_data(struct cerebrod_node_data *nd,
                                     u_int32_t received_time)
{
#if CEREBRO_DEBUG
  int rv;
#endif /* CEREBRO_DEBUG */

  assert(nd);
  assert(received_time);

  if (!event_index)
    return;

  assert(event_node_timeout_data_index);

#if CEREBRO_DEBUG
  /* Should be called with lock already set */
  rv = Pthread_mutex_trylock(&listener_data_lock);
  if (rv != EBUSY)
    CEREBRO_EXIT(("mutex not locked: rv=%d", rv));
#endif /* CEREBRO_DEBUG */

  Pthread_mutex_lock(&event_node_timeout_data_lock);
  /* Re-hash if our hash is getting too small */
  if ((event_node_timeout_data_index_numnodes + 1) > (event_node_timeout_data_index_size*2))
    cerebrod_rehash(&event_node_timeout_data_index,
                    &event_node_timeout_data_index_size,
                    EVENT_NODE_TIMEOUT_SIZE_INCREMENT,
                    event_node_timeout_data_index_numnodes,
                    &event_node_timeout_data_lock);

  _event_node_timeout_data_add(nd->nodename, received_time);
  event_node_timeout_data_index_numnodes++;
  Pthread_mutex_unlock(&event_node_timeout_data_lock);
}

void 
cerebrod_event_update_node_received_time(struct cerebrod_node_data *nd,
                                         u_int32_t received_time)
{
  struct cerebrod_event_node_timeout *ntd;
#if CEREBRO_DEBUG
  int rv;
#endif /* CEREBRO_DEBUG */

  assert(nd);
  assert(received_time);

  if (!event_index)
    return;

#if CEREBRO_DEBUG
  /* Should be called with lock already set */
  rv = Pthread_mutex_trylock(&nd->node_data_lock);
  if (rv != EBUSY)
    CEREBRO_EXIT(("mutex not locked: rv=%d", rv));
#endif /* CEREBRO_DEBUG */

  Pthread_mutex_lock(&event_node_timeout_data_lock);
  if ((ntd = Hash_find(event_node_timeout_data_index, nd->nodename)))
    {
      ntd->last_received_time = received_time;
      ntd->timeout_occurred = 0;
    }
  Pthread_mutex_unlock(&event_node_timeout_data_lock);
}

void
cerebrod_event_modules_update(const char *nodename,
                              struct cerebrod_node_data *nd,
                              const char *metric_name,
                              struct cerebrod_heartbeat_metric *hd)
{
  struct cerebrod_event_module_info *event_module;
  List event_list; 
#if CEREBRO_DEBUG
  int rv;
#endif /* CEREBRO_DEBUG */
  
  assert(nodename && nd && metric_name && hd);

  if (!event_index)
    return;

#if CEREBRO_DEBUG
  /* Should be called with lock already set */
  rv = Pthread_mutex_trylock(&nd->node_data_lock);
  if (rv != EBUSY)
    CEREBRO_EXIT(("mutex not locked: rv=%d", rv));
#endif /* CEREBRO_DEBUG */

  if ((event_list = Hash_find(event_index, metric_name)))
    {
      struct cerebro_event *event = NULL;
      int rv;

      ListIterator itr = NULL;

      itr = List_iterator_create(event_list);
      while ((event_module = list_next(itr)))
	{
	  Pthread_mutex_lock(&event_module->event_lock);
	  if ((rv = event_module_metric_update(event_handle,
                                               event_module->index,
                                               nodename,
                                               metric_name,
                                               hd->metric_value_type,
                                               hd->metric_value_len,
                                               hd->metric_value,
                                               &event)) < 0)
            {
              CEREBRO_DBG(("event_module_metric_update"));
              goto loop_next;
            }

          if (rv && event)
            cerebrod_queue_event(event, event_module->index);

        loop_next:
	  Pthread_mutex_unlock(&event_module->event_lock);
	}
      List_iterator_destroy(itr);
    }
}

#endif /* !WITH_CEREBROD_SPEAKER_ONLY */

