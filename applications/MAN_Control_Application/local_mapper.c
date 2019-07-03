/*
 * local_mapper.c
 *
 *  Created on: Jul 2, 2019
 *      Author: ruaro
 */
#include "communication.h"
#include <management_api.h>
#include <api.h>
#include "common_mapping.h"

/*Global Variables*/
unsigned int global_mapper_id = 0;
unsigned int cluster_addr = 0;


void initialize(){

	AddTaskLocation(global_mapper_id,global_mapper_id);

	unsigned int * message = get_message_slot();

	message[0] = INIT_I_AM_ALIVE;		//Task Service
	message[1] = GetNetAddress();	//Task Address

	Puts("Sending I AM ALIVE to global mapper\n");
	SendService(global_mapper_id, message, 2);
}

void initialize_ids(unsigned int * msg){

	int msg_index;
	unsigned int id, loc;

	cluster_addr =  msg[1];

	msg_index = 2;

	for(int i=0; i<=CLUSTER_NUMBER; i++){
		id = msg[msg_index++];
		loc = msg[msg_index++];
		AddTaskLocation(id, loc);
	}

	Puts("Local mapper initialized by global mapper, address = "); Puts(itoh(cluster_addr)); Puts("\n");

}


void handle_message(unsigned int * data_msg){

	switch (data_msg[0]) {
		case INIT_LOCAL:
			initialize_ids(data_msg);
			break;
		default:
			Puts("Error message unknown\n");
			for(;;);
			break;
	}
}

void main(){

	Echo("Initializing Local Mapper\n");
	RequestServiceMode();
	init_communication();
	initialize();

	unsigned int data_message[MAX_MAPPING_MSG];

	for(;;){

		ReceiveService(data_message);
		handle_message(data_message);

	}

	exit();
}


