/*****************************************************************************\
 *  $Id: cerebrod_config.c,v 1.5 2004-07-27 14:30:27 achu Exp $
\*****************************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#if HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */
#include <assert.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include "cerebrod_config.h"
#include "conffile.h"
#include "error.h"
#include "wrappers.h"

extern struct cerebrod_config conf;

void
cerebrod_config_default(void)
{
  conf.debug = 0;
  conf.configfile = CEREBROD_CONFIGFILE_DEFAULT;
  conf.listen = CEREBROD_LISTEN_DEFAULT;
  conf.speak = CEREBROD_SPEAK_DEFAULT;
  conf.heartbeat_freq_min = CEREBROD_HEARTBEAT_FREQ_MIN_DEFAULT;
  conf.heartbeat_freq_max = CEREBROD_HEARTBEAT_FREQ_MAX_DEFAULT;
  conf.network_interface = NULL;
  conf.mcast_ip = CEREBROD_MCAST_IP_DEFAULT;
  conf.mcast_port = CEREBROD_MCAST_PORT_DEFAULT;
  conf.mcast_listen_threads = CEREBROD_MCAST_LISTEN_THREADS_DEFAULT;
}

static void
_usage(void)
{
  fprintf(stderr, "Usage: cerebrod [OPTIONS]\n"
          "-h    --help        Output Help\n"
          "-v    --version     Output Version\n"
          "-c    --configfile  Specify alternate config file\n");
#ifndef NDEBUG
  fprintf(stderr, 
          "-d    --debug       Turn on debugging\n");
#endif /* NDEBUG */
  exit(0);
}

static void
_version(void)
{
  fprintf(stderr, "%s %s-%s\n", PROJECT, VERSION, RELEASE);
  exit(0);
}

void
cerebrod_cmdline_parse(int argc, char **argv)
{ 
  char c;
#ifndef NDEBUG
  char *options = "hvc:d";
#else
  char *options = "hvc:";
#endif

#if HAVE_GETOPT_LONG
  struct option long_options[] =
    {
      {"help",                0, NULL, 'h'},
      {"version",             0, NULL, 'v'},
      {"configfile",          1, NULL, 'c'},
#ifndef NDEBUG
      {"debug",               0, NULL, 'd'},
#endif /* NDEBUG */
    };
#endif /* HAVE_GETOPT_LONG */

  /* turn off output messages */
  opterr = 0;

#if HAVE_GETOPT_LONG
  while ((c = getopt_long(argc, argv, options, long_options, NULL)) != -1)
#else
  while ((c = getopt(argc, argv, options)) != -1)
#endif
    {
      switch (c)
        {
        case 'h':       /* --help */
          _usage();
          break;
        case 'v':       /* --version */
          _version();
          break;
        case 'c':       /* --configfile */
          conf.configfile = Strdup(optarg);
          break;
#ifndef NDEBUG
        case 'd':       /* --debug */
          conf.debug = 1;
          break;
#endif /* NDEBUG */
        case '?':
        default:
          err_exit("unknown command line option '%c'", optopt);
        }          
    }
}

static int
_cb_heartbeat_freq(conffile_t cf, struct conffile_data *data,
                   char *optionname, int option_type, void *option_ptr,
                   int option_data, void *app_ptr, int app_data)
{
  /* No need to check length of list, conffile checks it for you */

  if (data->intlist[0] > data->intlist[1])
    {
      conffile_seterrnum(cf, CONFFILE_ERR_PARAMETERS);
      return -1;
    }
  conf.heartbeat_freq_min = data->intlist[0];
  conf.heartbeat_freq_max = data->intlist[1];
  return 0;
}

static int
_cb_stringptr(conffile_t cf, struct conffile_data *data,
              char *optionname, int option_type, void *option_ptr,
              int option_data, void *app_ptr, int app_data)
{
  if (option_ptr == NULL)
    {
      conffile_seterrnum(cf, CONFFILE_ERR_PARAMETERS);
      return -1;
    }

  *((char **)option_ptr) = Strdup(data->string);
  return 0;
}

void
cerebrod_config_parse(void)
{
  int heartbeat_freq_flag, listen_flag, speak_flag, network_interface_flag, 
    mcast_ip_flag, mcast_port_flag, mcast_listen_threads_flag;

  struct conffile_option options[] =
    {
      {"listen", CONFFILE_OPTION_BOOL, -1, conffile_bool,
       1, 0, &listen_flag, &conf.listen, 0},
      {"speak", CONFFILE_OPTION_BOOL, -1, conffile_bool,
       1, 0, &speak_flag, &conf.speak, 0},
      {"heartbeat_freq", CONFFILE_OPTION_LIST_INT, 2, _cb_heartbeat_freq,
       1, 0, &heartbeat_freq_flag, NULL, 0},
      {"network_interface", CONFFILE_OPTION_STRING, -1, _cb_stringptr,
       1, 0, &network_interface_flag, &(conf.network_interface), 0},
      {"mcast_ip", CONFFILE_OPTION_STRING, -1, _cb_stringptr,
       1, 0, &mcast_ip_flag, &(conf.mcast_ip), 0},
      {"mcast_port", CONFFILE_OPTION_INT, -1, conffile_int,
       1, 0, &mcast_port_flag, &(conf.mcast_port), 0},
      {"mcast_listen_threads", CONFFILE_OPTION_INT, -1, conffile_int,
       1, 0, &mcast_listen_threads_flag, &(conf.mcast_listen_threads), 0},
    };
  conffile_t cf = NULL;
  int num;

  if ((cf = conffile_handle_create()) == NULL) 
    err_exit("conffile_handle_create: failed to create handle");

  num = sizeof(options)/sizeof(struct conffile_option);
  if (conffile_parse(cf, conf.configfile, options, num, NULL, 0, 0) < 0)
    {
      /* Its not an error if the file doesn't exist */
      if (conffile_errnum(cf) != CONFFILE_ERR_EXIST)
        {
          char buf[CONFFILE_MAX_ERRMSGLEN];
          if (conffile_errmsg(cf, buf, CONFFILE_MAX_ERRMSGLEN) < 0)
            err_exit("conffile_parse: errnum = %d", conffile_errnum(cf));
          err_exit("config file error: %s", buf);
        }
    }

  conffile_handle_destroy(cf);
}

/* From Unix Network Programming, by R. Stevens, Chapter 16 */
static int
_get_ifr_len(struct ifreq *ifr)
{
  int len;

#if HAVE_SA_LEN
  if (sizeof(struct sockaddr) > ifr->ifr_addr.sa_len)
    len = sizeof(struct sockaddr);
  else
    len = ifr->ifr_addr.sa_len;
#else /* !HAVE_SA_LEN */
  /* For now we only assume AF_INET and AF_INET6 */
  switch(ifr->ifr_addr.sa_family) {
#ifdef HAVE_IPV6
  case AF_INET6:
    len = sizeof(struct sockaddr_in6);
    break;
#endif /* HAVE_IPV6 */
  case AF_INET:
  default:
    len = sizeof(struct sockaddr_in);
    break;
  }

  if (len < (sizeof(struct ifreq) - IFNAMSIZ))
    len = sizeof(struct ifreq) - IFNAMSIZ;
#endif /* HAVE_SA_LEN */

  return len;
}

void
cerebrod_calculate_configuration(void)
{
  /* Step 1: Determine the appropriate network interface to use based
   * on the user's input
   */

  if (!conf.network_interface)
    {
      /* Case A: No interface specified, find any multicast interface */
      struct ifconf ifc;
      struct ifreq *ifr;
      int lastlen = -1, fd, len = sizeof(struct ifreq) * 100;
      void *buf = NULL, *ptr = NULL;
                                                                                    
      /* From Unix Network Programming, by R. Stevens, Chapter 16 */
                                                                                    
      fd = Socket(AF_INET, SOCK_DGRAM, 0);
                                                                                    
      /* get all active interfaces */
      while(1) 
	{
	  buf = Malloc(len);
                                                                                    
	  ifc.ifc_len = len;
	  ifc.ifc_buf = buf;
                                                                                    
	  if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) 
	    err_exit("cerebrod_calculate_configuration: ioctl: %s",
		     strerror(errno));
	  else 
	    {
	      if (ifc.ifc_len == lastlen)
		break;
	      lastlen = ifc.ifc_len;
	    }
	  
	  len += 10 * sizeof(struct ifreq);
	  Free(buf);
	}

      /* Check all interfaces */
      for (ptr = buf; ptr < buf + ifc.ifc_len; ) 
	{
	  struct ifreq ifr_tmp;
	  
	  ifr = (struct ifreq *)ptr;
                                                                                    
	  len = _get_ifr_len(ifr);
                                                                                    
	  ptr += sizeof(ifr->ifr_name) + len;
	  
	  /* Null termination not required, don't use wrapper */
	  strncpy(ifr_tmp.ifr_name, ifr->ifr_name, IFNAMSIZ);

	  if (ioctl(fd, SIOCGIFFLAGS, &ifr_tmp) < 0) 
	    err_exit("ioctl: %s", strerror(errno));
	  
	  if (!(ifr_tmp.ifr_flags & IFF_UP)
	      || !(ifr_tmp.ifr_flags & IFF_MULTICAST))
	    continue;

	  conf.multicast_interface = Strdup(ifr_tmp.ifr_name);
	  break;
	}
      
      Free(buf);
    }
  else if (strchr(conf.network_interface, '.'))
    {
      /* Case B: IP address specified */
      struct ifconf ifc;
      struct ifreq *ifr;
      int lastlen = -1, fd, len = sizeof(struct ifreq) * 100;
      void *buf = NULL, *ptr = NULL;
                                                                                    
      if (strchr(conf.network_interface, ':'))
	err_exit("IPv6 IP addresses currently not supported, please specify"
		 "IPv4 IP address");

      /* From Unix Network Programming, by R. Stevens, Chapter 16 */
                                                                                    
      fd = Socket(AF_INET, SOCK_DGRAM, 0);
                                                                                    
      /* get all active interfaces */
      while(1) 
	{
	  buf = Malloc(len);
                                                                                    
	  ifc.ifc_len = len;
	  ifc.ifc_buf = buf;
                                                                                    
	  if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) 
	    err_exit("cerebrod_calculate_configuration: ioctl: %s",
		     strerror(errno));
	  else 
	    {
	      if (ifc.ifc_len == lastlen)
		break;
	      lastlen = ifc.ifc_len;
	    }
	  
	  len += 10 * sizeof(struct ifreq);
	  Free(buf);
	}

      /* Check all interfaces */
      for (ptr = buf; ptr < buf + ifc.ifc_len; ) 
	{
	  struct ifreq ifr_tmp;
	  char *addr;

	  ifr = (struct ifreq *)ptr;
                                                                                    
	  len = _get_ifr_len(ifr);
                                                                                    
	  ptr += sizeof(ifr->ifr_name) + len;
	  
	  /* Currently no IPv6 support */
	  if (ifr->ifr_addr.sa_family != AF_INET)
	    continue;

	  if (!_compare_ip());

	  /*
	  sin = (struct sockaddr_in *)&ifr->ifr_addr;
                                                                                   
	  addr = inet_ntoa(sin->sin_addr);
	  if (strcmp(addr,"127.0.0.1") == 0)
	    continue;
	  
	  if (memcmp(munge_addr, (void *)&sin->sin_addr.s_addr, addr_len) == 0) {
	    found++;
	    break;
	  }
	  */

	  /* Null termination not required, don't use wrapper */
	  strncpy(ifr_tmp.ifr_name, ifr->ifr_name, IFNAMSIZ);

	  if (ioctl(fd, SIOCGIFFLAGS, &ifr_tmp) < 0) 
	    err_exit("ioctl: %s", strerror(errno));
	  
	  if (!(ifr_tmp.ifr_flags & IFF_UP))
	    continue;
	  
	  if (!(ifr_tmp.ifr_flags & IFF_MULTICAST))
	    continue;

	  conf.multicast_interface = Strdup(ifr_tmp.ifr_name);
	  break;
	}
      
      Free(buf);
    }
  else
    {
      /* Case C: Interface name specified */
      struct ifreq ifr;
      int fd;

      fd = Socket(AF_INET, SOCK_DGRAM, 0);
      Strncpy(ifr.ifr_name, conf.network_interface, IFNAMSIZ);

      if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) 
	{
	  if (errno == ENODEV)
	    err_exit("network interface '%s' not found",
		     conf.network_interface);
	  else
	    err_exit("ioctl: %s", strerror(errno));
	}

      if (!(ifr.ifr_flags & IFF_UP))
	  err_exit("network interface '%s' not up",
		   conf.network_interface);
      
      if (!(ifr.ifr_flags & IFF_MULTICAST))
	  err_exit("network interface '%s' not a multicast interface",
		   conf.network_interface);

      conf.multicast_interface = Strdup(conf.network_interface);

      Close(fd);
    }
}
