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

#include "task_communication.h"

#include "utils.h"
#include "../../include/services.h"
#include "../../hal/mips/HAL_kernel.h"
#include "enforcer_mapping.h"
#include "enforcer_migration.h"
#include "enforcer_sdn.h"


MessageRequest message_request[REQUEST_SIZE];	//!< message request array

unsigned int 	averange_latency = 500;				//!< Stores the averange latency

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

/** Useful function to writes a message into the task page space
 * \param task_tcb_ptr TCB pointer of the task
 * \param msg_lenght Lenght of the message to be copied
 * \param msg_data Message data
 */
void write_local_msg_to_task(TCB * consumer_tcb_ptr, int msg_lenght, int * msg_data){

	Message * msg_ptr;

	msg_ptr = (Message*) consumer_tcb_ptr->recv_buffer;

	msg_ptr->length = msg_lenght;

	for (int i=0; i<msg_ptr->length; i++)
		msg_ptr->msg[i] = msg_data[i];

	HAL_release_waiting_task(consumer_tcb_ptr);
	//puts("Task not waitining anymeore 3\n");

	//CTP reconfiguration
	check_ctp_reconfiguration(consumer_tcb_ptr);

	//task_tcb_ptr->communication_time = HAL_get_tick() - task_tcb_ptr->communication_time;
	consumer_tcb_ptr->total_comm += HAL_get_tick() - consumer_tcb_ptr->communication_time;
	//puts("["); puts(itoa(task_tcb_ptr->id)); puts("] Comm ==> "); puts(itoa(current->communication_time)); puts("\n");


}

/** Assembles and sends a MESSAGE_REQUEST packet to producer task into a slave processor
 * \param producer_task Producer task ID (Send())
 * \param consumer_task Consumer task ID (Receive())
 * \param targetPE Processor address of the producer task
 * \param requestingPE Processor address of the consumer task
 * \param insert_pipe_flag Tells to the producer PE that the message not need to be by passed again
 */
void send_message_request(int producer_task, int consumer_task, unsigned int targetPE, unsigned int requestingPE){

	ServiceHeader *p;
	int subnet;

	subnet = get_subnet(producer_task, consumer_task, DMNI_RECEIVE_OP);

	if ( subnet != -1){

		HAL_set_CS_request((1 << subnet));

	} else {

		p = get_service_header_slot();

		p->header = targetPE;

		p->service = MESSAGE_REQUEST;

		p->requesting_processor = requestingPE;

		p->producer_task = producer_task;

		p->consumer_task = consumer_task;

		send_packet(p, 0, 0);
	}

}


/** Assembles and sends a MESSAGE_DELIVERY packet to a consumer task located into a slave processor
 *  \param producer_task ID of the task that produce the message (Send())
 *  \param consumer_task ID of the task that consume the message (Receive())
 *  \param msg_ptr Message pointer
 */
void send_message_delivery(int producer_task, int consumer_task, int consumer_PE, Message * msg_ptr){

	ServiceHeader *p;
	int subnet = get_subnet(producer_task, consumer_task, DMNI_SEND_OP);

	if (subnet == -1){ //The communication is by PS

		p = get_service_header_slot();

		p->header = consumer_PE;

		p->service = MESSAGE_DELIVERY;

		p->producer_task = producer_task;

		p->consumer_task = consumer_task;

		p->msg_lenght = msg_ptr->length;

		send_packet(p, (unsigned int)msg_ptr->msg, msg_ptr->length);

	} else { //the communication is by CS
		//putsv("Send Delivery by subnet ", subnet);
		//The msg structure is send integrally including its lenght
		DMNI_send_data((unsigned int)msg_ptr, (msg_ptr->length + 1), subnet);

	}

}


int handle_message_request(volatile ServiceHeader * p){
	int producer_PE;
	TCB * prod_tcb_ptr;
	Message * prod_send_msg;

	//putsv("\nMESSAGE_REQUEST recebido da consumer at time: ", HAL_get_tick());
	//Gets the location of the producer task
	producer_PE = get_task_location(p->producer_task);

	//If the producer task still executing here
	if (producer_PE == net_address){

		//Test if the producer task have something produced for consumer

		//Gets TCB of producer task
		prod_tcb_ptr = searchTCB(p->producer_task);

		while (prod_tcb_ptr == 0){
			puts("ERROR prod_tcb_ptr is suposed to be something\n");
		}

		//Test if the producer task already produced something to the consumer, sending the message_delivery
		if (prod_tcb_ptr->send_buffer != 0 && prod_tcb_ptr->send_target == p->consumer_task){

			prod_send_msg = (Message *) prod_tcb_ptr->send_buffer;

			send_message_delivery(p->producer_task, p->consumer_task, p->requesting_processor, prod_send_msg);

			//Waits the message be tranfered
			while(HAL_is_send_active(PS_SUBNET));

			//Sets the producer buffer info free
			prod_tcb_ptr->send_buffer = 0;

			prod_tcb_ptr->send_target = -1;

			//Release the producer to run,
			//This line make the send function to return 1

			HAL_release_waiting_task(prod_tcb_ptr);

			//putsv("Enviando MESSAGE_DELIVERY, producer release at time: ", HAL_get_tick());

			return 1;//Need scheduling since prod was released

		} else { //If not, stores the message request
			//putsv("Request armazenado: ", HAL_get_tick());
			insert_message_request(p->producer_task, p->consumer_task, p->requesting_processor);
		}

	} else { //This means that task was migrated to another PE since its location is diferent

		//MESSAGE_REQUEST by pass
		send_message_request(p->producer_task, p->consumer_task, producer_PE, p->requesting_processor);

	}

	return 0;

}

int handle_message_delivery(volatile ServiceHeader * p, int subnet){

	int need_scheduling_ret = 0;
	int latency;
	TCB * cons_tcb_ptr;
	Message * recv_msg_ptr;

	//putsv("\nMESSAGE_DELIVERY recebido da producer at time: ", HAL_get_tick());

	cons_tcb_ptr = searchTCB(p->consumer_task);

	while (cons_tcb_ptr->recv_buffer == 0 || cons_tcb_ptr->recv_source != p->producer_task){
		puts("ERROR message delivery send to an invalid producer\n");
	}

	recv_msg_ptr = (Message *)(cons_tcb_ptr->recv_buffer);

	if (subnet == PS_SUBNET){ //Read by PS

		if(cons_tcb_ptr->scheduling_ptr->period)
			latency = HAL_get_tick()-p->timestamp;

		//In PS get the msg_lenght from service header
		recv_msg_ptr->length = p->msg_lenght;

		DMNI_read_data((unsigned int)recv_msg_ptr->msg, recv_msg_ptr->length);


#if ENABLE_LATENCY_MISS
		if(cons_tcb_ptr->scheduling_ptr->period){

			//putsv("Avg latency = ", averange_latency);
			//putsv("Latency = ", latency);

			if(latency > (averange_latency + (averange_latency/2))){

				send_latency_miss(cons_tcb_ptr, p->producer_task, p->source_PE);

				//puts("Latency miss: "); puts(itoa(p->producer_task)); puts(" [");puts(itoh(p->source_PE));puts(" -> "); puts(itoa(cons_tcb_ptr->id));puts(" [");puts(itoh(net_address));
				//puts("] = "); puts(itoa(latency)); puts("\n");

			} else {

				averange_latency = (averange_latency + latency) / 2;

			}

		}
#endif

	} else { //Read by CS

		//In CS get the msg_lenght from the NoC data
		recv_msg_ptr->length = DMNI_read_data_CS((unsigned int)recv_msg_ptr->msg, subnet);

	}

	//Release the consumer buffer info
	cons_tcb_ptr->recv_buffer = 0;
	cons_tcb_ptr->recv_source = -1;

	HAL_release_waiting_task(cons_tcb_ptr);

	//putsv("Liberou consumer to execute at time: ", HAL_get_tick());
	//puts("Task not waitining anymeore 1\n");

	check_ctp_reconfiguration(cons_tcb_ptr);

#if MIGRATION_ENABLED
	if (cons_tcb_ptr->proc_to_migrate != -1){
		//puts("Migrou no delivery\n");
		migrate_dynamic_memory(cons_tcb_ptr);
		need_scheduling_ret = 1;

	} else
#endif

	//putsv("Delivery ", HAL_get_tick());
	//puts("Delivery\n");
	cons_tcb_ptr->total_comm += HAL_get_tick() - cons_tcb_ptr->communication_time;
	//puts("["); puts(itoa(cons_tcb_ptr->id)); puts("] Comm ==> "); puts(itoa(cons_tcb_ptr->total_comm)); puts("\n");


	return need_scheduling_ret;
}

int send_message(TCB * running_task, unsigned int msg_addr, unsigned int consumer_task){

	unsigned int producer_task;
	unsigned int appID;
	int consumer_PE;
	int subnet_ret;
	Message * prod_msg_ptr;
	MessageRequest * msg_req_ptr;

	producer_task =  running_task->id;

	appID = producer_task >> 8;
	//Consumer task already has the absolute task id, this line creates the full id: app id | task id
	consumer_task = (appID << 8) | consumer_task;

	//puts("WRITEPIPE - prod: "); puts(itoa(producer_task)); putsv(" consumer ", consumer_task);

	consumer_PE = get_task_location(consumer_task);

	//Test if the consumer task is not allocated
	if (consumer_PE == -1){
		//Task is blocked until its a TASK_RELEASE packet
		running_task->scheduling_ptr->status = BLOCKED;
		return 0;
	}

	while (running_task->send_buffer != 0){
		puts("ERROR send buffer not zero\n");
	}

	/*Points the message in the task page. Address composition: offset + msg address*/
	running_task->send_buffer = (running_task->offset) | msg_addr;
	/*Set the consumer id of the message */
	running_task->send_target = consumer_task;

	prod_msg_ptr = (Message *) running_task->send_buffer;

	//Searches if there is a message request to the produced message
	msg_req_ptr = remove_message_request(producer_task, consumer_task);
	//puts("Remove message request\n");

	if (msg_req_ptr){ // If there is a message request for that message

		if (msg_req_ptr->requester_proc == net_address){ //Test if the consumer is local or remote

			//Writes to the consumer page address (local consumer)
			TCB * requesterTCB = searchTCB(consumer_task);

			while (requesterTCB->recv_buffer == 0 || requesterTCB->recv_source != producer_task){
				puts("ERROR recv buffer not zero\n");
			}

			//puts("Message found - Write local message\n");
			write_local_msg_to_task(requesterTCB, prod_msg_ptr->length, prod_msg_ptr->msg);

			/*Erase the produced message info since the message was transmitted*/
			running_task->send_buffer = 0;
			running_task->send_target = -1;
#if MIGRATION_ENABLED
			if (requesterTCB->proc_to_migrate != -1){

				migrate_dynamic_memory(requesterTCB);

				HAL_enable_scheduler_after_syscall();
			}
#endif

		} else { //remote request task

			//*************** Deadlock avoidance: avoids to send a packet when the DMNI is busy in send process ************************
			subnet_ret = get_subnet(producer_task, consumer_task, DMNI_SEND_OP);
			if (subnet_ret == -1)
				subnet_ret = PS_SUBNET; //If the CTP was not found, then by default send by PS
			if (HAL_is_send_active(subnet_ret)){
				//Restore the message request
				msg_req_ptr->requested = producer_task;
				msg_req_ptr->requester = consumer_task;
				return 0;
			}
			//**********************************************************

			//puts("Message found - send message delivery\n");
			send_message_delivery(producer_task, consumer_task, msg_req_ptr->requester_proc, prod_msg_ptr);

			/*Erase the produced message info since the message was transmitted*/
			running_task->send_buffer = 0;
			running_task->send_target = -1;

			//putsv("\nSENDMESSAGE - Enviando delivery at time: ", HAL_get_tick());

			//This is to avoid that the producer task overwrites the send_buffer before message is sent
			while (HAL_is_send_active(PS_SUBNET));

		}
	} else { //message not requested yet, set producer as waiting

		//putsv("\nSENDMESSAGE - producer wating at time: ", HAL_get_tick());


		running_task->scheduling_ptr->waiting_msg = 1;

		//Cals syscall in order to make other task to execute
		HAL_enable_scheduler_after_syscall();
	}

	return 1;

}


int receive_message(TCB * running_task, unsigned int msg_addr, unsigned int producer_task){

	unsigned int consumer_task;
	unsigned int appID;
	int producer_PE;
	int subnet_ret;
	TCB * prod_tcb_ptr;
	Message * prod_msg_ptr;

	consumer_task =  running_task->id;

	appID = consumer_task >> 8;
	producer_task = (appID << 8) | producer_task;

	//puts("READPIPE - prod: "); puts(itoa(producer_task)); putsv(" consumer ", consumer_task);

	producer_PE = get_task_location(producer_task);

	//Test if the producer task is not allocated
	if (producer_PE == -1){
		//Task is blocked until its a TASK_RELEASE packet
		running_task->scheduling_ptr->status = BLOCKED;
		return 0;
	}

	running_task->recv_buffer = running_task->offset | msg_addr;
	running_task->recv_source = producer_task;


	if (producer_PE == net_address){ //Receive is local

		prod_tcb_ptr = searchTCB(producer_task);

		if (prod_tcb_ptr->send_target == consumer_task && prod_tcb_ptr->send_buffer != 0){

			prod_msg_ptr = (Message*) prod_tcb_ptr->send_buffer;

			//puts("Local receive - write message\n");

			write_local_msg_to_task(running_task, prod_msg_ptr->length, prod_msg_ptr->msg);

			//Clear the receive buffer info since message was received
			running_task->recv_buffer = 0;
			running_task->recv_source = -1;

			//Clear the producer buffer info since message was transmitted
			prod_tcb_ptr->send_buffer = 0;
			prod_tcb_ptr->send_target = -1;

			//Alows producer to restore its execution after Send, since the message was transmitted
			HAL_release_waiting_task(prod_tcb_ptr);

			return 1; //Exit the receive returnin 1

		} else {
			//puts("Local receive - set message request table\n");
			insert_message_request(producer_task, consumer_task, net_address);
		}

	} else { //If the receive is from a remote proc

		//*************** Deadlock avoidance ************************
		//Só testa se tiver algo na rede PS pq o CS vai via sinal de req
		subnet_ret = get_subnet(producer_task, consumer_task, DMNI_RECEIVE_OP);
		if (subnet_ret == -1 && HAL_is_send_active(PS_SUBNET) ) {
			return 0;
		}
		//***********************************************************
		//puts("Remote receive - send message request\n");
		send_message_request(producer_task, consumer_task, producer_PE, net_address);

	}

	running_task->scheduling_ptr->waiting_msg = 1;

	HAL_enable_scheduler_after_syscall();

	//Used for DAPE purposes
	running_task->communication_time = HAL_get_tick();


	//putsv("\nRECVMESSAGE - consumer waiting at time: ", running_task->communication_time);

	//running_task->computation_time = running_task->communication_time - running_task->computation_time;
	running_task->total_comp += running_task->communication_time - running_task->computation_time;
	//puts("["); puts(itoa(running_task->id)); puts("] Comp --> "); puts(itoa(running_task->computation_time)); puts("\n");

	return 0;
}



void send_IO(){
//TODO

}
void receive_IO(){
//TODO
}

int write_local_service_to_MA(unsigned int consumer_id, unsigned int * msg_data, int msg_lenght){

	TCB * task_tcb_ptr;
	unsigned int * msg_address_tgt;

	task_tcb_ptr = searchTCB(consumer_id);

	if (task_tcb_ptr == 0){
		putsv("ERROR: tcb at SERVICE_TASK_MESSAGE_DELIVERY is null: ", task_tcb_ptr->id);
		while(1);
	}
	if (task_tcb_ptr->is_service_task == 0){
		puts("ERROR: Task at SERVICE_TASK_MESSAGE_DELIVERY is not a service task\n");
		while(1);
	}

	//If target is not waiting message return 0 and schedules target to it consume the message
	if (task_tcb_ptr->scheduling_ptr->waiting_msg == 0){
		return 0;
	}

	msg_address_tgt = (unsigned int *) (task_tcb_ptr->offset | task_tcb_ptr->reg[3]);

	//Copies the message from producer to consumer
	for(int i=0; i<msg_lenght; i++){
		msg_address_tgt[i] = msg_data[i];
	}

	HAL_release_waiting_task(task_tcb_ptr);
	//puts("Task not waitining anymeore 2\n");

	return 1;
}

void send_service_to_MA(int consumer_task, unsigned int targetPE, unsigned int * src_message, unsigned int msg_size){

	ServiceHeader * p = get_service_header_slot();

	p->header = targetPE;

	//p->service = SERVICE_TASK_MESSAGE;
	p->service = src_message[0]; //Service

	//This if is only usefull for graphical debugger, to it can monitor TASK_TERMINATED packets properly
	if(p->service == TASK_TERMINATED){
		p->task_ID = src_message[1];
	}

	p->consumer_task = consumer_task;

	p->msg_lenght = msg_size;

	/*putsv("Msg lenght: ", p->msg_lenght);
	putsv("targetPE: ", targetPE);
	for(int j=0; j<p->msg_lenght; j++){
		putsv("",src_message[j]);
	}*/

	send_packet(p, (unsigned int) src_message, msg_size);

	//SE ISSO ESTIVER DESCOMENTANDO ENTAO O CONTROLE DE SOBRESCRITA DE MEMORIA E FEITA PELA APLICACAO
	//while(HAL_is_send_active(PS_SUBNET));

}

void handle_MA_message(unsigned int consumer_task, unsigned int msg_size){

	TCB * consumer_tcb_ptr;

	consumer_tcb_ptr = searchTCB(consumer_task);

	//Por esse motivo cada execução de uma MA task nunca pode ser interrompida
	while (consumer_tcb_ptr->recv_buffer == 0 || consumer_tcb_ptr->scheduling_ptr->waiting_msg == 0){
		puts("ERROR message delivery send to an invalid producer\n");
	}

	DMNI_read_data(consumer_tcb_ptr->recv_buffer, msg_size);

	consumer_tcb_ptr->recv_buffer = 0;

	HAL_release_waiting_task(consumer_tcb_ptr);

}

//Muito similar ao send_message
int send_MA(TCB * running_task, unsigned int msg_addr, unsigned int msg_size, unsigned int consumer_task){

	unsigned int producer_task;
	int consumer_PE;
	unsigned int appID;
	unsigned int * prod_data, * cons_data;

	//Scheduler after syscall because the producer cannot send the message
	HAL_enable_scheduler_after_syscall();

	if (HAL_is_send_active(PS_SUBNET)){
		return 0;
	}

	/*if (!running_task->is_service_task){
		puts("Task is not a service task YET\n");
		return 0;
	}*/

	/*putsv("msg_addr: ", msg_addr);
	putsv("msg_size: ", msg_size);
	putsv("Prod_task: ", running_task->id);
	putsv("Consumer task: ", consumer_task);*/


	producer_task = running_task->id;
	appID = producer_task >> 8;
	consumer_task = (appID << 8) | consumer_task;

	consumer_PE = get_task_location(consumer_task);

	//Test if the consumer task is not allocated
	if (consumer_PE == -1){
		//Task is blocked until its a TASK_RELEASE packet
		running_task->scheduling_ptr->status = BLOCKED;
		return 0;
	}

	if (consumer_PE == net_address){

		TCB * consumer_tcb_ptr = searchTCB(consumer_task);

		//This mean that task is with a message pending to handle, set Send to sleep
		//When a producer can writes on recv_buffer it set it as zero to signal to another producers (local producers) that the buffer was fullfuled
		//if (consumer_tcb_ptr->recv_buffer == 0 || consumer_tcb_ptr->scheduling_ptr->waiting_msg == 0){
		if (consumer_tcb_ptr->recv_buffer == 0){
			//Set the producer as waiting task

			running_task->send_buffer = running_task->offset | msg_addr;
			running_task->send_target = msg_size; //Reuse of send_target to store send size

			//Return zero because Send cannot be performed, Send not enter in waiting because there is not a message_request to wakeup it
			return 0;

		} else { //If the local buffer is valid writes on it


			//Get the addresses
			prod_data = (unsigned int *) running_task->send_buffer;
			cons_data = (unsigned int *) consumer_tcb_ptr->recv_buffer;

			//Poderia usar essa funcao mas ela precisa ser melhorada
			//write_local_service_to_MA(consumer_tcb_ptr->id, prod_data, msg_size);

			//memcopy
			for(int i=0; i<msg_size; i++){
				prod_data[i] = cons_data[i];
			}

			//Mark that consumer was populated with a message or not called yet
			consumer_tcb_ptr->recv_buffer = 0;

			//Release consumer to run
			HAL_release_waiting_task(consumer_tcb_ptr);

			//Clear the buffer of producer
			running_task->send_buffer = 0;
			running_task->send_target = -1;

		}


	} else { //If consumer is in a remote PE then send the data throught the network

		prod_data = (unsigned int *) (running_task->offset | msg_addr);

		send_service_to_MA(consumer_task, consumer_PE, prod_data, msg_size);

	}

	return 1;

}

//Sets task to wait
void receive_MA(TCB * running_task, unsigned int msg_addr){

	while(running_task->recv_buffer != 0){
		puts("ERROR recv buffer MA should be zero\n");
	}

	running_task->recv_buffer = (running_task->offset | msg_addr);

	running_task->scheduling_ptr->waiting_msg = 1;

	HAL_enable_scheduler_after_syscall();

}

