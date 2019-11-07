/*
 * local_mapper.h
 *
 *  Created on: Jul 4, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_GLOBALS_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_GLOBALS_H_

#include "../../include/kernel_pkg.h"

/*Definitions*/
#define MAX_TASKS_APP			10
#define MAX_CLUSTER_TASKS		(MAX_LOCAL_TASKS * MAPPING_XCLUSTER * MAPPING_YCLUSTER)
#define MAX_PROCESSORS			(MAPPING_XCLUSTER*MAPPING_YCLUSTER)
#define CONSTANT_PKT_SIZE		13
#define PS_SUBNET 				(SUBNETS_NUMBER-1)

/*Global Variables*/
unsigned int 	cluster_position = 0;					//!< Stores the XY cluster positioning
unsigned int 	my_task_ID = 0;							//!< Stores the local mapper ID at task level
unsigned int 	cluster_x_offset;						//!< Stores the X address of the most left-bottom router
unsigned int 	cluster_y_offset;						//!< Stores the Y address of the most left-bottom router
unsigned int 	pending_app_to_map = 0; 				//!< Controls the number of pending applications already handled by not completely mapped
unsigned int 	free_resources = (MAX_CLUSTER_TASKS); 	//!< Controls the number of free pages for all processor of clusters


#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_GLOBALS_H_ */
