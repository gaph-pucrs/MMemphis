/*
 * local_mapper.h
 *
 *  Created on: Jul 4, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_GLOBALS_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_GLOBALS_H_

/*Definitions*/
#define MAX_TASKS_APP			20
#define MAX_CLUSTER_TASKS		(MAX_LOCAL_TASKS * XCLUSTER * YCLUSTER)
#define MAX_PROCESSORS			(XCLUSTER*YCLUSTER)

/*Global Variables*/
unsigned int 	global_mapper_id = 0;
unsigned int 	cluster_position = 0;
unsigned int 	cluster_x_offset;
unsigned int 	cluster_y_offset;
unsigned int 	pending_app_to_map = 0; 					//!< Controls the number of pending applications already handled by not completely mapped
unsigned int 	free_resources = (MAX_CLUSTER_TASKS);


#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_GLOBALS_H_ */
