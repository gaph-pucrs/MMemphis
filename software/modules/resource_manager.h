/*!\file resource_manager.h
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief This module defines the function relative to mapping a new app and a new task into a given slave processor.
 * This module is only used by manager kernel
 */

#ifndef RESOURCE_MANAGER_H_
#define RESOURCE_MANAGER_H_

#define get_cluster_proc(x)		( (cluster_info[x].master_x << 8) | cluster_info[x].master_y)

extern unsigned int clusterID; //!<Cluster ID index of array cluster_info - clusterID is the only extern global variable in in all software

inline void allocate_cluster_resource(int, int);

inline void release_cluster_resources(int, int);

void page_used(int, int, int);

void page_released(int, int, int);

int get_master_address(int);

int map_task(int, int);

int application_mapping(int, int);

int SearchCluster(int, int);

void set_cluster_ID(unsigned int);

int diamond_search(int, int, int);

#endif /* SOFTWARE_INCLUDE_RESOURCE_MANAGER_H_ */
