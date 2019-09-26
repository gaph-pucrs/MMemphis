/*
 * communication.h
 *
 *  Created on: 26 de set de 2018
 *      Author: mruaro
 */

#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include "noc_manager.h"
#include <management_api.h>

#define SDN_MSG_SIZE (XDIMENSION*YDIMENSION*CS_NETS)

#define DETAILED_ROUTING_REQUEST	1 //A message sent from the coordinator controller to the other controller that belongs to the paths
#define DETAILED_ROUTING_RESPONSE	2 //A message sent from the controllers to the coordintor controller informing about the sucess of failure of detailed routing
#define TOKEN_REQUEST				3 //A message sent from the coordinator controller to the token coordinator asking for the token grant
#define TOKEN_RELEASE				4 //A message sent from the coordinator releasing the token
#define TOKEN_GRANT					5 //A message sent from the token coordinator passing the token to the requesting coordinator
#define UPDATE_BORDER_REQUEST		6 //A message sent from the coordinator to all controller updating the status of border input
#define UPDATE_BORDER_ACK			7 //A message sent from any controller to its respective coordinator informating that it alread update the status of border
#define LOCAL_RELEASE_REQUEST		9 //A message sent from a controller to another in order to release all router of the path
#define LOCAL_RELEASE_ACK			10 //A message sent from each controler to the coordinator, informing that it release the path and passing which router was released
#define GLOBAL_MODE_RELEASE			11 //A message sent from the coordinator to all controller cancelling the its detailed routing
#define GLOBAL_MODE_RELEASE_ACK		12 //A message sent from each controller to the coordinator informing that it received the order to cancel the datailed routing

typedef struct {

	unsigned int message[SDN_MSG_SIZE];
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

	if (!(destination & TO_KERNEL))//Only change the destination if it is not to kernel
		//Aqui ocorre o truque de converter endereco de cluster em ID de tarefa
		destination = tx*CLUSTER_Y_NUMBER + ty;

	//Puts("Package sent to ID: "); Puts(itoa(destination)); Puts("\n");

	SendService(destination, msg, msg_uint_size);
}



#endif /* COMMUNICATION_H_ */
