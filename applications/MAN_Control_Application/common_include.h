/*
 * mapping.h
 *
 *  Created on: Jul 1, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_COMMON_INCLUDE_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_COMMON_INCLUDE_H_

#include "../../include/kernel_pkg.h"
#include "../../software/include/services.h"
#include "../../software/include/stdlib.h"
#include <api.h>
#include <management_api.h>


/*Definitions*/
#define CLUSTER_X_SIZE  	(XDIMENSION/XCLUSTER)
#define	CLUSTER_Y_SIZE		(YDIMENSION/YCLUSTER)


#define putsv(string, value) Puts(string); Puts(itoa(value)); Puts("\n");

#define MAX_MAPPING_MSG		100


typedef struct {

	unsigned int message[MAX_MAPPING_MSG];
	unsigned int status;

}SDNMessageSlot;

volatile SDNMessageSlot sdn_msg_slot1, sdn_msg_slot2;	//!<Slots to prevent memory writing while is sending a packet

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


/*Common Useful functions*/
void send_task_allocation_message(unsigned int task_id, unsigned int allocated_proc, unsigned int master_addr){

	unsigned int * message;

	message = get_message_slot();
	message[0] = APP_INJECTOR; //Destination
	message[1] = 4; //Packet size
	message[2] = APP_ALLOCATION_REQUEST; //Service
	message[3] = task_id; //Repository task ID
	message[4] = master_addr; //Master address
	message[5] = allocated_proc;

	//Send message to Peripheral
	SendRaw(message, 6);

	Puts("Requesting task "); Puts(itoa(task_id)); Puts(" allocated at proc "); Puts(itoh(allocated_proc));
	Puts(" belowing to master "); Puts(itoh(master_addr)); Puts("\n");

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
	destination = (tx + ty*(XDIMENSION/XCLUSTER)) + 1; //PLus 1 because the global mapper uses ID 0

	//Puts("Package sent to ID: "); Puts(itoa(destination)); Puts("\n");
	SendService(destination, msg, msg_uint_size);
}


#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_COMMON_INCLUDE_H_ */
