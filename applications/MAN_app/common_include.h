/*
 * mapping.h
 *
 *  Created on: Jul 1, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_COMMON_INCLUDE_H_
#define APPLICATIONS_MAN_APP_COMMON_INCLUDE_H_

#include "../../include/kernel_pkg.h"
#include "../../software/include/services.h"
#include "../../software/include/stdlib.h"
#include <api.h>
#include <management_api.h>


//CAUTION: the size of XCLUSTER and YCLUSTER must be always divisible (remaining 0) with XDIMENSION and YDIMENSION

#define MAPPING_XCLUSTER		XCLUSTER
#define MAPPING_YCLUSTER		YCLUSTER
#define MAPPING_X_CLUSTER_NUM	(XDIMENSION/MAPPING_XCLUSTER)
#define MAPPING_Y_CLUSTER_NUM	(YDIMENSION/MAPPING_YCLUSTER)
#define MAPPING_CLUSTER_NUM		(MAPPING_X_CLUSTER_NUM*MAPPING_Y_CLUSTER_NUM)
#define MAX_MAPPING_TASKS		(MAPPING_CLUSTER_NUM+1)//Plus one due global mapper


#define SDN_XCLUSTER			XCLUSTER
#define	SDN_YCLUSTER			YCLUSTER
#define SDN_X_CLUSTER_NUM		(XDIMENSION/SDN_XCLUSTER)
#define SDN_Y_CLUSTER_NUM		(YDIMENSION/SDN_YCLUSTER)
#define	MAX_SDN_TASKS			(SDN_X_CLUSTER_NUM*SDN_Y_CLUSTER_NUM)


#define putsv(string, value) Puts(string); Puts(itoa(value)); Puts("\n");

//#define MAX_MAPPING_MSG		100
//#define MAX_MANAG_MSG_SIZE (XDIMENSION*YDIMENSION*CS_NETS)
#define MAX_MANAG_MSG_SIZE	100


typedef struct {

	unsigned int message[MAX_MANAG_MSG_SIZE];
	unsigned int status;

}MangmtMessageSlot;

volatile 		MangmtMessageSlot sdn_msg_slot1, sdn_msg_slot2;	//!<Slots to prevent memory writing while is sending a packet
unsigned int 	global_task_ID = 0; 							//!< Stores the global mapper ID at task level


unsigned int * get_message_slot() {

	if ( sdn_msg_slot1.status ) {

		sdn_msg_slot1.status = 0;
		sdn_msg_slot2.status = 1;
		return (unsigned int *) &sdn_msg_slot1.message;

	} else {

		sdn_msg_slot2.status = 0;
		sdn_msg_slot1.status = 1;
		return (unsigned int *) &sdn_msg_slot2.message;
	}
}

void init_message_slots(){
    sdn_msg_slot1.status = 1;
    sdn_msg_slot2.status = 1;
}


void initialize_MA_task(){

	unsigned int * message = get_message_slot();

	AddTaskLocation(global_task_ID,global_task_ID);

	message[0] = INIT_I_AM_ALIVE;		//Task Service
	message[1] = GetNetAddress();	//Task Address
	message[2] = GetMyID();
	//Puts("Sending I AM ALIVE to global mapper\n");
	SendService(global_task_ID, message, 3);
}


int position_to_ID(unsigned int pos){
	int tx, ty;

	tx = pos >> 8;
	ty = pos & 0xFF;

	//Converts address in ID
	pos = (tx + ty*(XDIMENSION/XCLUSTER)) + 1; //PLus 1 because the global mapper uses ID 0

	return pos;
}


/** This function abstract the communication among local mappers converting destination (0x1, 1x0 ...) to task IDs
 * It only works between local mappers. To communcate with global mapper use the SendService
 */
void send(unsigned int destination, unsigned int * msg, int msg_uint_size){
	int tx, ty;

	tx = destination >> 8;
	ty = destination & 0xFF;

	//Converts address in ID
	//if (!(destination & TO_KERNEL))//Only change the destination if it is not to kernel
	destination = (tx + ty*(XDIMENSION/XCLUSTER)) + 1; //PLus 1 because the global mapper uses ID 0

	//Puts("Package sent to ID: "); Puts(itoa(destination)); Puts("\n");
	SendService(destination, msg, msg_uint_size);
}


#endif /* APPLICATIONS_MAN_APP_COMMON_INCLUDE_H_ */
