/*
 * request_queue.h
 *
 *  Created on: 7 de nov de 2018
 *      Author: ruaro
 */

#ifndef GLOBAL_CONNECTION_REQUEST_QUEUE_H_
#define GLOBAL_CONNECTION_REQUEST_QUEUE_H_

#include "connection_request.h"

#define GLOBAL_REQ_QUEUE_SIZE		NC_NUMBER

unsigned int global_req_first = 0;	//!<first valid array index
unsigned int global_req_last = 0;	//!<last valid array index

unsigned char global_token_add_fifo = 0; 	//!<Keeps the last operation: 1 - last operation was add. 0 - last operation was remove

ConnectionRequest global_request_connection_FIFO[GLOBAL_REQ_QUEUE_SIZE];	//!<request_connection array declaration

/**Add a new CS requesst
 * \param request Unsinged integer value with the address of source and target PE
 * \return 1 if add was OK, 0 is the array is full (ERROR situation, while forever)
 */
int add_global_connection_request(unsigned int source, unsigned int target, unsigned int requester_address, int subnet){

	ConnectionRequest * fifo_free_position;
	
	//Test if the buffer is full
	if (global_req_first == global_req_last && global_token_add_fifo == 1){
		Puts("ERROR: Global Request FIFO FULL\n");
		while(1);
		return 0;
	}

	fifo_free_position = &global_request_connection_FIFO[global_req_last];
	
	fifo_free_position->source = source;
	fifo_free_position->target = target;
	fifo_free_position->requester_address = requester_address;
	fifo_free_position->subnet = subnet;
	

	if (global_req_last == GLOBAL_REQ_QUEUE_SIZE-1){
		global_req_last = 0;
	} else {
		global_req_last++;
	}

	global_token_add_fifo = 1;

	return 1;
}

/**Searches by the next pending service, remove then, and return
 * \return The removed ServiceHeader pointer. As it is a memory position, the received part will do something with this pointer and discard the
 * reference
 */
ConnectionRequest * get_next_global_connection_request(){

	ConnectionRequest * removed_request;

	//Test if the buffer is empty
	if (global_req_first == global_req_last && global_token_add_fifo == 0){
		return 0;
	}

	removed_request = &global_request_connection_FIFO[global_req_first];

	if (global_req_first == GLOBAL_REQ_QUEUE_SIZE-1){
		global_req_first = 0;
	} else {
		global_req_first++;
	}

	global_token_add_fifo = 0;

	return removed_request;
}

#endif /* GLOBAL_CONNECTION_REQUEST_QUEUE_H_ */
