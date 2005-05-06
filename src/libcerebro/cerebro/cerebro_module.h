/*****************************************************************************\
 *  $Id: cerebro_module.h,v 1.1 2005-05-05 16:12:57 achu Exp $
\*****************************************************************************/

#ifndef _CEREBRO_MODULE_H
#define _CEREBRO_MODULE_H

#include <cerebro/cerebro_config.h>

/* 
 * cerebro_module_setup
 *
 * Initialize library for loading modules
 *
 * Returns 0 on success, -1 on error
 */
int cerebro_module_setup(void);

/* 
 * cerebro_module_is_setup
 *
 * Return 1 if module lib has been initialized, 0 if not
 */
int cerebro_module_is_setup(void);

/* 
 * cerebro_module_cleanup
 *
 * Cleanup library from loading modules
 *
 * Returns 0 on success, -1 on error
 */
int cerebro_module_cleanup(void);

/*
 * cerebro_module_load_clusterlist_module
 *
 * Find and load the clusterlist module.  If none is found, cerebro
 * library will assume a default clusterlist module.
 * 
 * Returns 1 if module is found and loaded, 0 if one isn't found and
 * the default is loaded, -1 on fatal error
 */
int cerebro_module_load_clusterlist_module(void);

/*
 * cerebro_module_clusterlist_unload
 *
 * Unload the clusterlist module.
 *
 * Returns 0 on success, -1 on error
 */
int cerebro_module_unload_clusterlist_module(void);

/*
 * cerebro_module_load_config_module
 *
 * Find and load the config module.  If none is found, cerebro
 * library will assume a default config module.
 * 
 * Returns 1 if module is found and loaded, 0 if one isn't found and
 * the default is loaded, -1 on fatal error
 */
int cerebro_module_load_config_module(void);

/*
 * cerebro_module_unload_config_module
 *
 * Unload the config module.
 *
 * Returns 0 on success, -1 on error
 */
int cerebro_module_unload_config_module(void);

/* 
 * cerebro_module_clusterlist_module_is_loaded
 * 
 * Return 1 if a clusterlist module has been loaded, 0 if not
 */
int cerebro_module_clusterlist_module_is_loaded(void);

/* 
 * cerebro_module_config_module_is_loaded
 * 
 * Return 1 if a config module has been loaded, 0 if not
 */
int cerebro_module_config_module_is_loaded(void);

/* 
 * cerebro_module_clusterlist_module_found
 * 
 * Return 1 if a clusterlist module was found, 0 if we are using the default
 */
int cerebro_module_clusterlist_module_found(void);

/* 
 * cerebro_module_config_module_found
 * 
 * Return 1 if a config module module was found, 0 if we are using the default
 */
int cerebro_module_config_module_found(void);

/*
 * cerebro_clusterlist_module_name
 *
 * Return clusterlist module name
 */
char *cerebro_clusterlist_module_name(void);

/*
 * cerebro_clusterlist_module_setup
 *
 * call clusterlist module setup function
 */
int cerebro_clusterlist_module_setup(void);
                                                                                     
/*
 * cerebro_clusterlist_module_cleanup
 *
 * call clusterlist module parse cleanup function
 */
int cerebro_clusterlist_module_cleanup(void);
                                                                                     
/*
 * cerebro_clusterlist_module_get_all_nodes
 *
 * call clusterlist module get all nodes function
 */
int cerebro_clusterlist_module_get_all_nodes(char **nodes, 
					     unsigned int nodeslen);

/*
 * cerebro_clusterlist_module_numnodes
 *
 * call clusterlist module numnodes function
 */
int cerebro_clusterlist_module_numnodes(void);
                                                                                     
/*
 * cerebro_clusterlist_module_node_in_cluster
 *
 * call clusterlist module node in cluster function
 */
int cerebro_clusterlist_module_node_in_cluster(const char *node);
                                                                                     
/*
 * cerebro_clusterlist_module_get_nodename
 *
 * call clusterlist module get nodename function
 */
int cerebro_clusterlist_module_get_nodename(const char *node, 
					    char *buf, 
					    unsigned int buflen);

/*
 * cerebro_config_module_name
 *
 * Return config module name
 */
char *cerebro_config_module_name(void);
                                                                                     
/*
 * cerebro_config_module_setup
 *
 * call config module setup function
 */
int cerebro_config_module_setup(void);
                                                                                     
/*
 * cerebro_config_module_cleanup
 *
 * call config module parse cleanup function
 */
int cerebro_config_module_cleanup(void);
                                                                                     
/*
 * cerebro_config_module_load_default
 *
 * call config module get all nodes function
 */
int cerebro_config_module_load_default(struct cerebro_config *conf);

#endif /* _CEREBRO_MODULE_H */