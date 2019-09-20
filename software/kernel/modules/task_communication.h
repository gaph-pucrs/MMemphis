/*!\file communication.h
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief Defines the PipeSlot and MessageRequest structures.
 *
 * \detailed
 * PipeSlot stores the user's messages produced by not consumed yet.
 * MessageRequest stores the requested messages send to the consumer task by not produced yet
 */


#ifndef SOFTWARE_INCLUDE_COMMUNICATION_COMMUNICATION_H_
#define SOFTWARE_INCLUDE_COMMUNICATION_COMMUNICATION_H_

#include "../../hal/mips/HAL_kernel.h"
#include "../../include/api.h"
#include "packet.h"
#include "TCB.h"


#define REQUEST_SIZE	 MAX_LOCAL_TASKS*(MAX_TASKS_APP-1) //50	//!< Size of the message request array in fucntion of the maximum number of local task and max task per app

/**
 * \brief This structure stores the message requests used to implement the blocking Receive MPI
 */
typedef struct {
    int requester;             	//!< Store the requested task id ( task that performs the Receive() API )
    int requested;             	//!< Stores the requested task id ( task that performs the Send() API )
    int requester_proc;			//!< Stores the requester processor address
} MessageRequest;


void init_communication();

int insert_message_request(int, int, int);

int search_message_request(int, int);

MessageRequest * remove_message_request(int, int);

int remove_all_requested_msgs(int, unsigned int *);

int send_message(TCB *, unsigned int, unsigned int);

int receive_message(TCB *, unsigned int, unsigned int);

void send_message_request(int, int, unsigned int, unsigned int);

void send_message_delivery(int, int, int, Message *);

int handle_message_delivery(volatile ServiceHeader *, int);

int handle_message_request(volatile ServiceHeader *);

/*MA API functions*/
int send_MA(TCB *, unsigned int, unsigned int, unsigned int);

void receive_MA(TCB *, unsigned int);

void handle_MA_message(unsigned int, unsigned int);

void send_service_to_MA(int, unsigned int, unsigned int *, unsigned int);

void write_local_service_to_MA(unsigned int, unsigned int *, int);


#endif /* SOFTWARE_INCLUDE_COMMUNICATION_COMMUNICATION_H_ */
