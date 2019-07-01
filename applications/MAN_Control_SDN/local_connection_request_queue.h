/*
 * request_queue.h
 *
 *  Created on: 7 de nov de 2018
 *      Author: ruaro
 */

#ifndef LOCAL_CONNECTION_REQUEST_QUEUE_H_
#define LOCAL_CONNECTION_REQUEST_QUEUE_H_

#include "../MAN_SDN/connection_request.h"

#define LOCAL_REQ_QUEUE_SIZE		NC_NUMBER

unsigned int local_req_first = 0;	//!<first valid array index
unsigned int local_req_last = 0;	//!<last valid array index

unsigned char local_token_add_fifo = 0; 	//!<Keeps the last operation: 1 - last operation was add. 0 - last operation was remove

ConnectionRequest local_request_connection_FIFO[LOCAL_REQ_QUEUE_SIZE];	//!<request_connection array declaration

/**Add a new CS requesst
 * \param request Unsinged integer value with the address of source and target PE
 * \return 1 if add was OK, 0 is the array is full (ERROR situation, while forever)
 */
int add_local_connection_request(unsigned int source, unsigned int target, unsigned int requester_address, int subnet){

	ConnectionRequest * fifo_free_position;
	
	//Test if the buffer is full
	if (local_req_first == local_req_last && local_token_add_fifo == 1){
		Puts("ERROR: Local Request FIFO FULL\n");
		while(1);
		return 0;
	}

	fifo_free_position = &local_request_connection_FIFO[local_req_last];
	
	fifo_free_position->source = source;
	fifo_free_position->target = target;
	fifo_free_position->requester_address = requester_address;
	fifo_free_position->subnet = subnet;
	

	if (local_req_last == LOCAL_REQ_QUEUE_SIZE-1){
		local_req_last = 0;
	} else {
		local_req_last++;
	}

	local_token_add_fifo = 1;

	return 1;
}

/**Searches by the next pending service, remove then, and return
 * \return The removed ServiceHeader pointer. As it is a memory position, the received part will do something with this pointer and discard the
 * reference
 */
ConnectionRequest * get_next_local_connection_request(){

	ConnectionRequest * removed_request;

	//Test if the buffer is empty
	if (local_req_first == local_req_last && local_token_add_fifo == 0){
		return 0;
	}

	removed_request = &local_request_connection_FIFO[local_req_first];

	if (local_req_first == LOCAL_REQ_QUEUE_SIZE-1){
		local_req_first = 0;
	} else {
		local_req_first++;
	}

	local_token_add_fifo = 0;

	return removed_request;
}

#endif /* LOCAL_CONNECTION_REQUEST_QUEUE_H_ */
