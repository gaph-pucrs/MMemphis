/*
 * communication.h
 *
 *  Created on: Jul 2, 2019
 *      Author: ruaro
 */

#include <management_api.h>
#include "common_mapping.h"

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_COMMUNICATION_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_COMMUNICATION_H_

#define MAX_MAPPING_MSG		100

/*Internal Services*/
#define		INIT_I_AM_ALIVE	0
#define		INIT_LOCAL		1


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

void init_communication(){
    sdn_msg_slot1.status = 1;
    sdn_msg_slot2.status = 1;
}


void send(unsigned int destination, unsigned int * msg, int msg_uint_size){
	int tx, ty;
	//char dest[20] = "noc_manager";
	//int index = 11;

	tx = destination >> 8;
	ty = destination & 0xFF;

	//Converts address in ID
	destination = tx + ty*(XDIMENSION/XCLUSTER);

	//Puts("Package sent to ID: "); Puts(itoa(destination)); Puts("\n");
	SendService(destination, msg, msg_uint_size);
}



#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_COMMUNICATION_H_ */
