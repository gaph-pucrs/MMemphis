/*
 * global_mapper.h
 *
 *  Created on: Jul 3, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_GLOBAL_MAPPER_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_GLOBAL_MAPPER_H_

typedef struct {
	int x_addr;					//!< Stores the application id
	int y_addr;					//!< Stores the application status
	unsigned int manager_addr;
} Cluster;



#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_GLOBAL_MAPPER_H_ */
