/*
 * mapping.h
 *
 *  Created on: Jul 1, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_COMMON_MAPPING_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_COMMON_MAPPING_H_

#include "../../include/kernel_pkg.h"
#include "../../software/include/services.h"
#include "../../software/include/stdlib.h"
#include "communication.h"
#include <api.h>
#include <management_api.h>


/*Definitions*/
#define CLUSTER_X_SIZE  	(XDIMENSION/XCLUSTER)
#define	CLUSTER_Y_SIZE		(YDIMENSION/YCLUSTER)


/*Common Useful functions*/
void send_task_allocation_message(unsigned int task_id, unsigned int allocated_proc){

	unsigned int * message;

	message = get_message_slot();
	message[0] = APP_INJECTOR; //Destination
	message[1] = 4; //Packet size
	message[2] = APP_ALLOCATION_REQUEST; //Service
	message[3] = task_id; //Repository task ID
	message[4] = 0; //Master address
	message[5] = allocated_proc;

	//Send message to Peripheral
	SendIO(message, 6);

	Puts("Task allocation sent to app_injector "); Puts(itoa(message[5] >> 8)); Puts("x"); Puts(itoa(message[5] & 0xFF)); Puts("\n");
}


#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_COMMON_MAPPING_H_ */
