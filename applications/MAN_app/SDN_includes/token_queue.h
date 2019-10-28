/*
 * token_requeu.h
 *
 *  Created on: 8 de nov de 2018
 *      Author: mruaro
 */

#ifndef TOKEN_QUEUE_H_
#define TOKEN_QUEUE_H_

#define TOKEN_QUEUE_SIZE		NC_NUMBER

unsigned int token_first;	//!<first valid array index
unsigned int token_last;	//!<last valid array index

unsigned char add_fifo; 	//!<Keeps the last operation: 1 - last operation was add. 0 - last operation was remove

int token;

unsigned int token_request_FIFO[TOKEN_QUEUE_SIZE];	//!<request_connection array declaration

/**Add a new CS requesst
 * \param request Unsinged integer value with the address of source and target PE
 * \return 1 if add was OK, 0 is the array is full (ERROR situation, while forever)
 */
int add_token_request(unsigned int requester_address){

	//Test if the buffer is full
	if (token_first == token_last && add_fifo == 1){
		Puts("ERROR: Pending service FIFO FULL\n");
		while(1);
		return 0;
	}

	token_request_FIFO[token_last] = requester_address;

	if (token_last == TOKEN_QUEUE_SIZE-1){
		token_last = 0;
	} else {
		token_last++;
	}

	add_fifo = 1;

	return 1;
}

/**Searches by the next pending service, remove then, and return
 * \return The removed ServiceHeader pointer. As it is a memory position, the received part will do something with this pointer and discard the
 * reference
 */
int get_next_token_request(){

	int removed_token;

	//Test if the buffer is empty
	if (token_first == token_last && add_fifo == 0){
		return -1;
	}

	removed_token = token_request_FIFO[token_first];

	if (token_first == TOKEN_QUEUE_SIZE-1){
		token_first = 0;
	} else {
		token_first++;
	}

	add_fifo = 0;

	return removed_token;
}

void init_token_queue(){
	token_first = 0;
	token_last = 0;

	add_fifo = 0;

	token = -1;
}



#endif /* TOKEN_QUEUE_H_ */
