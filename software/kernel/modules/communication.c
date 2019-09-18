/*!\file communication.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief Implements the PIPE and MessageRequest structures management.
 * This module is only used by slave kernel
 */

#include "communication.h"
#include "utils.h"

MessageRequest message_request[REQUEST_SIZE];	//!< message request array


/** Initializes the message request and the pipe array
 */
void init_communication(){

	for(int i=0; i<REQUEST_SIZE; i++){
		message_request[i].requested = -1;
		message_request[i].requester = -1;
		message_request[i].requester_proc = -1;
	}
}


/** Inserts a message request into the message_request array
 *  \param producer_task ID of the producer task of the message
 *  \param consumer_task ID of the consumer task of the message
 *  \param requester_proc Processor of the consumer task
 *  \return 0 if the message_request array is full, 1 if the message was successfully inserted
 */
int insert_message_request(int producer_task, int consumer_task, int requester_proc) {

    int i;

    for (i=0; i<REQUEST_SIZE; i++)
    	if ( message_request[i].requester == -1 ) {
    		message_request[i].requester  = consumer_task;
    		message_request[i].requested  = producer_task;
    		message_request[i].requester_proc = requester_proc;

    		//Only for debug purposes
    		//MemoryWrite(ADD_REQUEST_DEBUG, (producer_task << 16) | (consumer_task & 0xFFFF));

    		return 1;
		}

    puts("ERROR - request table if full\n");
	return 0;	/*no space in table*/
}

/** Searches for a message request
 *  \param producer_task ID of the producer task of the message
 *  \param consumer_task ID of the consumer task of the message
 *  \return 0 if the message was not found, 1 if the message was found
 */
int search_message_request(int producer_task, int consumer_task) {

    for(int i=0; i<REQUEST_SIZE; i++) {
        if( message_request[i].requested == producer_task && message_request[i].requester == consumer_task){
            return 1;
        }
    }

    return 0;
}

/** Remove a message request
 *  \param producer_task ID of the producer task of the message
 *  \param consumer_task ID of the consumer task of the message
 *  \return -1 if the message was not found or the requester processor address (processor of the consumer task)
 */
MessageRequest * remove_message_request(int producer_task, int consumer_task) {

    for(int i=0; i<REQUEST_SIZE; i++) {
        if( message_request[i].requested == producer_task && message_request[i].requester == consumer_task){
        	message_request[i].requester = -1;
        	message_request[i].requested = -1;

        	//Only for debug purposes
        	//MemoryWrite(REM_REQUEST_DEBUG, (producer_task << 16) | (consumer_task & 0xFFFF));

            //return message_request[i].requester_proc;
        	return &message_request[i];
        }
    }

    return 0;
}

/**Remove all message request of a requested task ID and copies such messages to the removed_msgs array.
 * This function is used for task migration only, when a task need to be moved to other processor
 *  \param requested_task ID of the requested task
 *  \param removed_msgs array pointer of the removed messages
 *  \return number of removed messages
 */
int remove_all_requested_msgs(int requested_task, unsigned int * removed_msgs){

	int request_index = 0;

	for(int i=0; i<REQUEST_SIZE; i++) {

		if (message_request[i].requested == requested_task){

			//Copies the messages to the array
			removed_msgs[request_index++] = message_request[i].requester;
			removed_msgs[request_index++] = message_request[i].requested;
			removed_msgs[request_index++] = message_request[i].requester_proc;

        	//Only for debug purposes
			//MemoryWrite(REM_REQUEST_DEBUG, (message_request[i].requester << 16) | (message_request[i].requested & 0xFFFF));

			//Removes the message request
			message_request[i].requester = -1;
			message_request[i].requested = -1;
		}
	}

	return request_index;
}
