/*!\file kernel_slave.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Edited by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * Kernel slave is the system slave used to execute user's tasks.
 *
 * \detailed
 * kernel_slave is the core of the OS running into the slave processors.
 * Its job is to runs the user's task. It communicates whit the kernel_master to receive new tasks
 * and also notifying its finish.
 * The kernel_slave file uses several modules that implement specific functions
 */

#include "../include/api.h"
#include "../include/management_api.h"
#include "../include/plasma.h"
#include "../include/services.h"
#include "kernel.h"
#include "modules/task_location.h"
#include "modules/packet.h"
#include "modules/communication.h"
#include "modules/pending_service.h"
#include "modules/ctp.h"
#include "modules/task_scheduler.h"
#include "modules/utils.h"
#if MIGRATION_ENABLED
#include "modules/task_migration.h"
#endif

//Globals
unsigned int 	schedule_after_syscall;		//!< Signals the syscall function (assembly implemented) to call the scheduler after the syscall
unsigned int 	interrput_mask;				//!< Stores the bit set allowing interruption, this variable is set within main()
unsigned int 	last_idle_time;				//!< Store the last idle time duration
unsigned int 	last_slack_time_report;		//!< Store time at the last slack time update sent to manager
unsigned int 	total_slack_time;			//!< Store the total of the processor idle time
TCB 			idle_tcb;					//!< TCB pointer used to run idle task
TCB *			current;					//!< TCB pointer used to store the current task executing into processor
Message 		msg_write_pipe;				//!< Message variable which is used to copy a message and send it by the NoC

unsigned short int	ctp_producer_adress;	//!< Used to set a CTP online
unsigned int last_task_profiling_update;	//!< Used to store the last time that the profiling was sent to manager
unsigned int averange_latency = 500;				//!< Stores the averange latency

/** Assembles and sends a TASK_TERMINATED packet to the master kernel
 *  \param terminated_task Terminated task TCB pointer
 */
//void send_task_terminated(TCB * terminated_task, int perc){/*<--- perc**apagar trecho de end simulation****/
void send_task_terminated(TCB * terminated_task){

	unsigned int * message[3];
	unsigned int master_addr, master_id;

	message[0] = TASK_TERMINATED;
	message[1] = terminated_task->id; //p->task_ID
	message[2] = terminated_task->master_address;

	master_addr = terminated_task->master_address & 0xFFFF;
	master_id = terminated_task->master_address >> 16;

	send_service_api_message(master_id, master_addr, message, 3);

	puts("Sent task TERMINATED to "); puts(itoh(master_addr)); puts("\n");
	putsv("Master id: ", master_id);

	while(is_send_active(PS_SUBNET));

	puts("Sended\n");


}

/** Assembles and sends a CTP_CS_ACK packet to the master kernel, signalizing
 * that the CS release or stablisment was accomplished
 *  \param producer_task Task ID of the CTP's producer task
 *  \param consumert_task Task ID of the CTP's consumer task
 *  \param subnet Subnet ID
 *  \param cs_mode Flag indicating to release or stablish a CS connection (1 - stablish, 0 - release)
 */
void send_NoC_switching_ack(int producer_task, int consumert_task, int subnet, int cs_mode){

	ServiceHeader * p;
	TCB * prod_tcb_ptr;

	prod_tcb_ptr = searchTCB(producer_task);

	if (!prod_tcb_ptr){
		puts("ERROR, not TCB found in send_cs_ack\n");
		for(;;);
	}

	p = get_service_header_slot();

	p->header = prod_tcb_ptr->master_address;

	p->service = NOC_SWITCHING_PRODUCER_ACK;

	p->producer_task = producer_task;

	p->consumer_task = consumert_task;

	p->cs_net = subnet;

	p->cs_mode = cs_mode;

	send_packet(p, 0, 0);


	//Sends the ACK to the consumer also
	if (cs_mode) {//Only send if the mode is establish

		//puts("Enviou NOC_SWITCHING_PRODUCER_ACK para consumer\n");
		p = get_service_header_slot();

		p->header = get_task_location(consumert_task);

		p->service = NOC_SWITCHING_PRODUCER_ACK;

		p->producer_task = producer_task;

		p->consumer_task = consumert_task;

		p->cs_net = subnet;

		p->cs_mode = cs_mode;

		send_packet(p, 0, 0);
	}

	//puts("send ACK to manager\n");
}

/** Assembles and sends a TASK_ALLOCATED packet to the master kernel
 *  \param allocated_task Allocated task TCB pointer
 */
void send_task_allocated(TCB * allocated_task){

	unsigned int master_addr, master_task_id;
	unsigned int * message[3];

	message[0] = TASK_ALLOCATED;
	message[1] = allocated_task->id;

	master_task_id = allocated_task->master_address >> 16;
	master_addr = allocated_task->master_address & 0xFFFF;

	send_service_api_message(master_task_id, master_addr, message, 2);

	while(is_send_active(PS_SUBNET));

	puts("Sending task allocated\n");
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

/** Assembles and sends a SLACK_TIME_REPORT packet to the master kernel
 */
void send_slack_time_report(){

	//Achei melhor nao desativar pra validar o SDN com tudo junto
	return;

	unsigned int time_aux;

	ServiceHeader * p = get_service_header_slot();

	p->header = cluster_master_address;

	p->service = SLACK_TIME_REPORT;

	time_aux = MemoryRead(TICK_COUNTER);

	p->cpu_slack_time = ( (total_slack_time*100) / (time_aux - last_slack_time_report) );

	//putsv("Slack time sent = ", (total_slack_time*100) / (time_aux - last_slack_time_report));

	total_slack_time = 0;

	last_slack_time_report = time_aux;

	send_packet(p, 0, 0);
}

/** Assembles and sends a UPDATE_TASK_LOCATION packet to a slave processor. Useful because task migration
 * \param target_proc Target slave processor which the packet will be sent
 * \param task_id Task ID that have its location updated
 * \param new_task_location New location (slave processor address) of the task
 */
void request_update_task_location(unsigned int target_proc, unsigned int task_id, unsigned int new_task_location){

	ServiceHeader *p = get_service_header_slot();
	int master_address;

	//Request to manager to update the task location for all tasks
	master_address = remove_pending_migration_task(task_id);

	if (master_address != -1)
		send_task_migrated(task_id, new_task_location, master_address);

	//In some cases with simultaneous task migration, the manager can sent the update location
	//to a old processor, in this case the onw slave update the task location
	if (target_proc != net_address){

		p->header = target_proc;

		p->service = UPDATE_TASK_LOCATION;

		p->task_ID = task_id;

		p->allocated_processor = new_task_location;

		send_packet(p, 0, 0);
	}

	if (target_proc != new_task_location){

		p = get_service_header_slot();

		p->header = new_task_location;

		p->service = UPDATE_TASK_LOCATION;

		p->task_ID = task_id;

		p->allocated_processor = new_task_location;

		send_packet(p, 0, 0);
	}

}

/** Assembles and sends a MESSAGE_REQUEST packet to producer task into a slave processor
 * \param producer_task Producer task ID (Send())
 * \param consumer_task Consumer task ID (Receive())
 * \param targetPE Processor address of the producer task
 * \param requestingPE Processor address of the consumer task
 * \param insert_pipe_flag Tells to the producer PE that the message not need to be by passed again
 */
void send_message_request(int producer_task, int consumer_task, unsigned int targetPE, unsigned int requestingPE, int insert_pipe_flag){

	ServiceHeader *p;
	int subnet;

	subnet = get_subnet(producer_task, consumer_task, DMNI_RECEIVE_OP);

	if ( subnet != -1){

		MemoryWrite(WRITE_CS_REQUEST, (1 << subnet));

	} else {

		p = get_service_header_slot();

		p->header = targetPE;

		p->service = MESSAGE_REQUEST;

		p->requesting_processor = requestingPE;

		p->producer_task = producer_task;

		p->consumer_task = consumer_task;

		p->insert_request = insert_pipe_flag;

		send_packet(p, 0, 0);
	}

}


void send_service_api_message(int consumer_task, unsigned int targetPE, unsigned int * src_message, unsigned int msg_size){

	ServiceHeader * p = get_service_header_slot();

	p->header = targetPE;

	p->service = SERVICE_TASK_MESSAGE;

	p->consumer_task = consumer_task;

	p->msg_lenght = msg_size;

	/*putsv("Msg lenght: ", p->msg_lenght);
	putsv("targetPE: ", targetPE);
	for(int j=0; j<p->msg_lenght; j++){
		putsv("",src_message[j]);
	}*/

	send_packet(p, (unsigned int) src_message, msg_size);

	//SE ISSO ESTIVER DESCOMENTANDO ENTAO O CONTROLE DE SOBRESCRITA DE MEMORIA E FEITA PELA APLICACAO
	//while(is_send_active(PS_SUBNET));

}

void send_latency_miss(TCB * target_task, int producer_task, int producer_proc){

	ServiceHeader * p = get_service_header_slot();

	p->header = target_task->master_address;

	p->service = LATENCY_MISS_REPORT;

	p->producer_task = producer_task;

	p->consumer_task = target_task->id;

	p->target_processor = producer_proc;

	send_packet(p, 0, 0);

}


void send_config_router(int target_proc, int input_port, int output_port, int cs_net){

  volatile unsigned int config_pkt[3];

	//5 is a value used by software to distinguish between the LOCAL_IN e LOCAL_OUT port
	//At the end of all, the port 5 not exists inside the router, and the value is configured to 4.
	if (input_port == 5)
		input_port = 4;
	else if (output_port == 5)
		output_port = 4;

	if (target_proc == net_address){

		config_subnet(input_port, output_port, cs_net);

	} else {

		config_pkt[0] = 0x10000 | target_proc;

		config_pkt[1] = 1;

		config_pkt[2] = (input_port << 13) | (output_port << 10) | (2 << cs_net) | 0;

		DMNI_send_data((unsigned int)config_pkt, 3 , PS_SUBNET);

		while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << PS_SUBNET));
	}
#if CS_DEBUG
	puts("\n********* Send config router ********* \n");
	puts("target_proc = "); puts(itoh(target_proc));
	puts("\ninput port = "); puts(itoa(input_port));
	puts("\noutput_port = "); puts(itoa(output_port));
	puts("\ncs_net = "); puts(itoa(cs_net)); puts("\n***********************\n");
#endif
}


/** Useful function to writes a message into the task page space
 * \param task_tcb_ptr TCB pointer of the task
 * \param msg_lenght Lenght of the message to be copied
 * \param msg_data Message data
 */
void write_local_msg_to_task(TCB * task_tcb_ptr, int msg_lenght, int * msg_data){

	Message * msg_ptr;

	msg_ptr = (Message*)((task_tcb_ptr->offset) | ((unsigned int)task_tcb_ptr->reg[3])); //reg[3] = address message

	msg_ptr->length = msg_lenght;

	for (int i=0; i<msg_ptr->length; i++)
		msg_ptr->msg[i] = msg_data[i];

	//Unlock the blocked task
	task_tcb_ptr->reg[0] = TRUE;

	//Set to ready to execute into scheduler
	task_tcb_ptr->scheduling_ptr->waiting_msg = 0;
	puts("Task not waitining anymeore 3\n");

	if (task_tcb_ptr->add_ctp)
		add_ctp_online(task_tcb_ptr);
	else if (task_tcb_ptr->remove_ctp)
		remove_ctp_online(task_tcb_ptr);


	//task_tcb_ptr->communication_time = MemoryRead(TICK_COUNTER) - task_tcb_ptr->communication_time;
	task_tcb_ptr->total_comm += MemoryRead(TICK_COUNTER) - task_tcb_ptr->communication_time;
	//puts("["); puts(itoa(task_tcb_ptr->id)); puts("] Comm ==> "); puts(itoa(current->communication_time)); puts("\n");


}

/** Syscall handler. It is called when a task calls a function defined into the api.h file
 * \param service Service of the syscall
 * \param arg0 Generic argument
 * \param arg1 Generic argument
 * \param arg2 Generic argument
 */
int Syscall(unsigned int service, unsigned int arg0, unsigned int arg1, unsigned int arg2) {

	Message *msg_read;
	Message *msg_write;
	PipeSlot *pipe_ptr;
	TCB * tcb_ptr;
	MessageRequest * msg_req_ptr;
	unsigned int * msg_address_src, * msg_address_tgt;
	int consumer_task;
	int producer_task;
	int producer_PE;
	int consumer_PE;
	int appID;
	int ret;


	schedule_after_syscall = 0;

	switch (service) {

		case EXIT:

			schedule_after_syscall = 1;

			//Deadlock avoidance: avoids to send a packet when the DMNI is busy in send process
			if (is_send_active(PS_SUBNET) || search_PIPE_producer(current->id)){
				return 0;
			}

			puts("Task id: "); puts(itoa(current->id)); putsv(" terminated at ", MemoryRead(TICK_COUNTER));

			//send_task_terminated(current, arg0);
			send_task_terminated(current);

			clear_scheduling(current->scheduling_ptr);

			appID = current->id >> 8;

			if ( !is_another_task_running(appID) ){

				clear_app_tasks_locations(appID);
			}

		return 1;

		case WRITEPIPE:

			producer_task =  current->id;
			consumer_task = (int) arg1;

			appID = producer_task >> 8;
			consumer_task = (appID << 8) | consumer_task;

			//puts("WRITEPIPE - prod: "); puts(itoa(producer_task)); putsv(" consumer ", consumer_task);

			consumer_PE = get_task_location(consumer_task);

			//Test if the consumer task is not allocated
			if (consumer_PE == -1){
				//Task is blocked until its a TASK_RELEASE packet
				current->scheduling_ptr->status = BLOCKED;
				return 0;
			}

			/*Points the message in the task page. Address composition: offset + msg address*/
			msg_read = (Message *)((current->offset) | arg0);

			//Searches if there is a message request to the produced message
			msg_req_ptr = remove_message_request(producer_task, consumer_task);

			if (msg_req_ptr){

				if (msg_req_ptr->requester_proc == net_address){ //Test if the consumer is local or remote

					//Writes to the consumer page address (local consumer)
					TCB * requesterTCB = searchTCB(consumer_task);

					write_local_msg_to_task(requesterTCB, msg_read->length, msg_read->msg);

#if MIGRATION_ENABLED
					if (requesterTCB->proc_to_migrate != -1){

						migrate_dynamic_memory(requesterTCB);

						schedule_after_syscall = 1;
					}
#endif

				} else { //remote request task

					//*************** Deadlock avoidance: avoids to send a packet when the DMNI is busy in send process ************************
					ret = get_subnet(producer_task, consumer_task, DMNI_SEND_OP);
					if (ret == -1)
						ret = PS_SUBNET; //If the CTP was not found, then by default send by PS
					if (is_send_active(ret)){
						//Restore the message request
						msg_req_ptr->requested = producer_task;
						msg_req_ptr->requester = consumer_task;
						return 0;
					}
					//**********************************************************

					msg_write_pipe.length = msg_read->length;

					//Avoids message overwriting by the producer task
					for (int i=0; i < msg_read->length; i++)
						msg_write_pipe.msg[i] = msg_read->msg[i];

					//Bug fixed, for some reason the consumerPE has being used as requeter_proc - changed to msg_req_prt->requester_proc
					send_message_delivery(producer_task, consumer_task, msg_req_ptr->requester_proc, &msg_write_pipe);

				}
			} else { //message not requested yet, stores into PIPE

				//########################### ADD PIPE #################################
				ret = add_PIPE(producer_task, consumer_task, msg_read);
				//########################### ADD PIPE #################################

				if (ret == 0){
					schedule_after_syscall = 1;
					return 0;
				}
			}

		return 1;

		case READPIPE:

			consumer_task =  current->id;
			producer_task = (int) arg1;

			appID = consumer_task >> 8;
			producer_task = (appID << 8) | producer_task;

			//puts("READPIPE - prod: "); puts(itoa(producer_task)); putsv(" consumer ", consumer_task);

			producer_PE = get_task_location(producer_task);

			//Test if the producer task is not allocated
			if (producer_PE == -1){
				//Task is blocked until its a TASK_RELEASE packet
				current->scheduling_ptr->status = BLOCKED;
				return 0;
			}


			if (producer_PE == net_address){

				pipe_ptr = remove_PIPE(producer_task, consumer_task);

				if (pipe_ptr == 0){

					insert_message_request(producer_task, consumer_task, net_address);

				} else {

					msg_write = (Message*) arg0;

					msg_write = (Message*)((current->offset) | ((unsigned int)msg_write));

					msg_write->length = pipe_ptr->message.length;

					for (int i = 0; i<msg_write->length; i++) {
						msg_write->msg[i] = pipe_ptr->message.msg[i];
					}

					return 1;
				}

			} else {

				//*************** Deadlock avoidance ************************
				//Só testa se tiver algo na rede PS pq o CS vai via sinal de req
				ret = get_subnet(producer_task, consumer_task, DMNI_RECEIVE_OP);
				if (ret == -1 && is_send_active(PS_SUBNET) ) {
					return 0;
				}
				//***********************************************************

				send_message_request(producer_task, consumer_task, producer_PE, net_address, 0);

			}

			current->scheduling_ptr->waiting_msg = 1;

			schedule_after_syscall = 1;

			current->communication_time = MemoryRead(TICK_COUNTER);
			//putsv("Request ", current->communication_time);

			//current->computation_time = current->communication_time - current->computation_time;
			current->total_comp += current->communication_time - current->computation_time;
			//puts("["); puts(itoa(current->id)); puts("] Comp --> "); puts(itoa(current->computation_time)); puts("\n");


			return 0;

		case GETTICK:

			return MemoryRead(TICK_COUNTER);

		case GETMYID:

			return current->id;

		case ECHO:

			//Header tag, used by Deloream
			puts("$$$_");
			puts(itoa(net_address>>8));puts("x");puts(itoa(net_address&0xFF)); puts("_");
			puts(itoa(current->id >> 8)); puts("_");
			puts(itoa(current->id & 0xFF)); puts("_");

			//Task message
			puts((char *)((current->offset) | (unsigned int) arg0));
			puts("\n");

		break;

		case REALTIME:

			if (is_send_active(PS_SUBNET)){
				return 0;
			}

			//putsv("\nReal-time to task: ", current->id);

			real_time_task(current->scheduling_ptr, arg0, arg1, arg2);

			send_real_time_constraints(current->scheduling_ptr);

			//schedule_after_syscall = 1;

			return 1;

		break;

		case SENDRAW:

			if(is_send_active(PS_SUBNET))
				return 0;

			msg_address_src = (unsigned int *) (current->offset | arg0);

			DMNI_send_data(msg_address_src, arg1, PS_SUBNET);

			while(is_send_active(PS_SUBNET));

			return 1;

		/************************* SERVICE API FUNCTIONS **************************/
		case REQSERVICEMODE:
			//TODO: a protocol that only grants a service permission to secure tasks
			current->is_service_task = 1;

			puts("Task REQUESTED service - disabling interruption\n");
			OS_InterruptMaskClear(0xffffffff);

			return 1;

		case WRITESERVICE:

			//puts("\nWRITESERVICE\n");

			//Enable to schedule after syscall and interruptions since task will be blocked anyway
			schedule_after_syscall = 1;

			//Test if the task has permission to use service API
			if (!current->is_service_task)
				return 0;

			//Extracts the ID of producer and consumer task
			producer_task =  current->id;
			consumer_task = (int) arg0;
			//putsv("Producer task: ", producer_task);
			//putsv("Consumer task: ", consumer_task);
			appID = producer_task >> 8;
			consumer_task = (appID << 8) | consumer_task;

			//Gets the PE location of the consumer task if the to_kernel mode is false
			if (arg0 & TO_KERNEL) {//Test if the address is to kernel
				consumer_PE = arg0 & 0xFFFF;
				putsv("Send to kernel: ", consumer_PE);
			} else
				consumer_PE = get_task_location(consumer_task);

			//Test if the consumer task is not allocated yet, this means that master does not sent TASK_RELEASE yet
			if (consumer_PE == -1){
				//Task is blocked until its a TASK_RELEASE packet
				current->scheduling_ptr->status = BLOCKED;
				puts("WARNING: Task blocked\n");
				return 0;
			}
			//Stores the address of producer task message
			msg_address_src = (unsigned int *) (current->offset | arg1);

			//If both the task are at same PE then:
			if (consumer_PE == net_address){

				puts("ERROR - Two service tasks mapped at same PE\n");
				putsv("consumerPE: ", consumer_PE);
				while(1);

			} else { //Else send the message to the remote task

				//if (is_send_active(PS_SUBNET))
				//	return 0;
				send_service_api_message(consumer_task, consumer_PE, msg_address_src, arg2);
			}

			return 1;

		case READSERVICE:

			//puts("\nREADSERVICE\n");

			schedule_after_syscall = 1;

			if (!current->is_service_task)
				return 0;

			//consumer_task =  current->id;
			//producer_task = (int) arg1;

			/*putsv("Address passed: ", arg0);
			putsv("Full address: ", (current->offset) | arg0);
			putsv("Offset: ", (current->offset));*/

			current->scheduling_ptr->waiting_msg = 1;
			//putsv("Task is waiting message: ", (unsigned int)current);

			OS_InterruptMaskSet(interrput_mask);

			return 0;

		case PUTS:

			puts((char *)((current->offset) | (unsigned int) arg0));

			return 1;

		case CFGROUTER:

			if (is_send_active(PS_SUBNET))
				return 0;

			send_config_router(arg0, arg1 >> 8, arg1 & 0xFF, arg2);

			return 1;
		case NOCSENDFREE:
			//If DMNI send is active them return FALSE
			if (is_send_active(PS_SUBNET))
				return 0;
			//Otherwise return TRUE
			return 1;

		case INCOMINGPACKET:
			return (MemoryRead(IRQ_STATUS) >= IRQ_INIT_NOC);

		case GETNETADDRESS:
			return net_address;
		case ADDTASKLOCATION:
			add_task_location(arg0, arg1);
			puts("Added task id "); puts(itoa(arg0)); puts(" at loc "); puts(itoh(arg1)); puts("\n");
			break;
		case REMOVETASKLOCATION:
			remove_task_location(arg0);
			break;
		case GETTASKLOCATION:
			return get_task_location(arg0);
		case SETMYID:
			current->id = arg0;
			putsv("Task id changed to: ", current->id);
			break;
	}

	return 0;
}

/** Handles a new packet from NoC
 */
int handle_packet(volatile ServiceHeader * p, unsigned int subnet) {

	int need_scheduling, code_lenght, app_ID, task_loc, latency;
	unsigned int app_tasks_location[MAX_TASKS_APP];
	PipeSlot * slot_ptr;
	Message * msg_ptr;
	unsigned int * tgt_message_addr;
	TCB * tcb_ptr = 0;
	CTP * ctp_ptr;

	need_scheduling = 0;

	switch (p->service) {

	case MESSAGE_REQUEST: //This case is the most complicated of the HeMPS if you understand it, so you understand all task communication protocol
	//This case sincronizes the communication messages also in case of task migration, the migration allows several scenarios, that are handled inside this case

		//Gets the location of the producer task
		task_loc = get_task_location(p->producer_task);

		/*if (task_loc == -1){
			puts("**** ERROR **** task location -1\n");
			while(1);
		}*/

		//Test if the task was migrated to this processor but have message produced in the old processor
		//In this case is necessary to forward the message request to the old processor
		if (searchTCB(p->producer_task) && task_loc != net_address){

			if (p->insert_request || task_loc == -1)
				insert_message_request(p->producer_task, p->consumer_task, p->requesting_processor);
			else
				//MESSAGE_REQUEST by pass
				send_message_request(p->producer_task, p->consumer_task, task_loc, p->requesting_processor, 0);

			break;
		}

		//Remove msg from PIPE, if there is no message, them slot_ptr will be 0
		//Note that this line, is below to the by pass above, becase we need to avoid that the a message be removed if still there are other messages in pipe of the old proc
		slot_ptr = remove_PIPE(p->producer_task, p->consumer_task);

		//Test if there is no message in PIPE
		if (slot_ptr == 0){

			//Test if the producer task is running at this processor, this conditions work togheter to the first by pass condition (above).
			if (task_loc == net_address){

				insert_message_request(p->producer_task, p->consumer_task, p->requesting_processor);

			} else { //If not, the task was migrated and the requester message_request need to be forwarded to the correct producer proc

				//If there is no more messages in the pipe of the producer, update the location of the migrated task at the remote proc
				if ( search_PIPE_producer(p->producer_task) == 0)
					request_update_task_location(p->requesting_processor, p->producer_task, task_loc);

				//MESSAGE_REQUEST by pass with flag insert in pipe enabled (there is no more message for the requesting task in this processor)
				send_message_request(p->producer_task, p->consumer_task, task_loc, p->requesting_processor, 1);
			}

		//message found, send it!!
		} else if (p->requesting_processor != net_address){

			send_message_delivery(p->producer_task, p->consumer_task, p->requesting_processor, &slot_ptr->message);

		//This else is executed when this slave received a own MESSAGE_REQUEST due a task migration by pass
		} else {

			tcb_ptr = searchTCB(p->consumer_task);

			write_local_msg_to_task(tcb_ptr, slot_ptr->message.length, slot_ptr->message.msg);
		}

		break;

	case  MESSAGE_DELIVERY:

		tcb_ptr = searchTCB(p->consumer_task);

		msg_ptr = (Message *)(tcb_ptr->offset | tcb_ptr->reg[3]);

		if (subnet == PS_SUBNET){ //Read by PS

			if(tcb_ptr->scheduling_ptr->period)
				latency = MemoryRead(TICK_COUNTER)-p->timestamp;

			//In PS get the msg_lenght from service header
			msg_ptr->length = p->msg_lenght;

			DMNI_read_data((unsigned int)msg_ptr->msg, msg_ptr->length);


#if ENABLE_LATENCY_MISS
			if(tcb_ptr->scheduling_ptr->period){

				//putsv("Avg latency = ", averange_latency);
				//putsv("Latency = ", latency);

				if(latency > (averange_latency + (averange_latency/2))){

					send_latency_miss(tcb_ptr, p->producer_task, p->source_PE);

					//puts("Latency miss: "); puts(itoa(p->producer_task)); puts(" [");puts(itoh(p->source_PE));puts(" -> "); puts(itoa(tcb_ptr->id));puts(" [");puts(itoh(net_address));
					//puts("] = "); puts(itoa(latency)); puts("\n");

				} else {

					averange_latency = (averange_latency + latency) / 2;

				}

			}
#endif

		} else { //Read by CS

			//In CS get the msg_lenght from the NoC data
			 msg_ptr->length = DMNI_read_data_CS((unsigned int)msg_ptr->msg, subnet);

		}

		tcb_ptr->reg[0] = 1;

		tcb_ptr->scheduling_ptr->waiting_msg = 0;
		//puts("Task not waitining anymeore 1\n");

		if (tcb_ptr->add_ctp)
			add_ctp_online(tcb_ptr);
		else if (tcb_ptr->remove_ctp)
			remove_ctp_online(tcb_ptr);

#if MIGRATION_ENABLED
		if (tcb_ptr->proc_to_migrate != -1){
			//puts("Migrou no delivery\n");
			migrate_dynamic_memory(tcb_ptr);
			need_scheduling = 1;

		} else
#endif

		//putsv("Delivery ", MemoryRead(TICK_COUNTER));
		//puts("Delivery\n");
		tcb_ptr->total_comm += MemoryRead(TICK_COUNTER) - tcb_ptr->communication_time;
		//puts("["); puts(itoa(tcb_ptr->id)); puts("] Comm ==> "); puts(itoa(tcb_ptr->total_comm)); puts("\n");


		if (current == &idle_tcb){
			need_scheduling = 1;
		}

		break;

	case TASK_ALLOCATION:

		tcb_ptr = search_free_TCB();

		tcb_ptr->pc = 0;

		tcb_ptr->id = p->task_ID;

		puts("Task id: "); puts(itoa(tcb_ptr->id)); putsv(" allocated at ", MemoryRead(TICK_COUNTER));

		code_lenght = p->code_size;

		tcb_ptr->text_lenght = code_lenght;

		tcb_ptr->master_address = p->master_ID;

		puts("Master address: "); puts(itoh(tcb_ptr->master_address)); puts("\n");

		tcb_ptr->remove_ctp = 0;

		tcb_ptr->add_ctp = 0;

		tcb_ptr->is_service_task = 0;

		tcb_ptr->proc_to_migrate = -1;

		tcb_ptr->scheduling_ptr->remaining_exec_time = MAX_TIME_SLICE;

		DMNI_read_data(tcb_ptr->offset, code_lenght);

		if ((tcb_ptr->id >> 8) == 0){//Task of APP 0 (mapping) dont need to be released to start its execution
			tcb_ptr->scheduling_ptr->status = READY;
		} else { //For others task
			tcb_ptr->scheduling_ptr->status = BLOCKED;
			send_task_allocated(tcb_ptr);
		}

		if (current == &idle_tcb){
			need_scheduling = 1;
		}

		break;

	case TASK_RELEASE:

		tcb_ptr = searchTCB(p->task_ID);

		//putsv("TASK_RELEASE\nTask ID: ", p->task_ID);

		app_ID = p->task_ID >> 8;

		tcb_ptr->data_lenght = p->data_size;

		//puts("Data lenght: "); puts(itoh(tcb_ptr->data_lenght)); puts("\n");

		tcb_ptr->bss_lenght = p->bss_size;

		//puts("BSS lenght: "); puts(itoh(tcb_ptr->data_lenght)); puts("\n");

		tcb_ptr->text_lenght = tcb_ptr->text_lenght - tcb_ptr->data_lenght;

		if (tcb_ptr->scheduling_ptr->status == BLOCKED){
			tcb_ptr->scheduling_ptr->status = READY;
		}

		//putsv("app task number: ", p->app_task_number);

		DMNI_read_data( (unsigned int) app_tasks_location, p->app_task_number);

		for (int i = 0; i < p->app_task_number; i++){
			add_task_location(app_ID << 8 | i, app_tasks_location[i]);
			//puts("Add task "); puts(itoa(app_ID << 8 | i)); puts(" loc "); puts(itoh(app_tasks_location[i])); puts("\n");
		}

		if (current == &idle_tcb){
			need_scheduling = 1;
		}

		break;

	case UPDATE_TASK_LOCATION:

		if (is_another_task_running(p->task_ID >> 8) ){

			remove_task_location(p->task_ID);

			add_task_location(p->task_ID, p->allocated_processor);
		}

		break;

	case INITIALIZE_SLAVE:

		cluster_master_address = p->source_PE;

		putsv("Slave initialized by cluster address: ", cluster_master_address);

		if (is_disturbing_prod_PE()){
			disturbing_generator();
		} else if (is_disturbing_cons_PE()){
			disturbing_reader();
		}

		app_ID = net_address >> 8;
		task_loc = net_address & 0xFF;
		app_ID += task_loc;

		last_task_profiling_update = (int)(APL_WINDOW/app_ID);
		//putsv("last_task_profiling_update = ", last_task_profiling_update);

		break;

#if MIGRATION_ENABLED
		case TASK_MIGRATION:
		case MIGRATION_CODE:
		case MIGRATION_TCB:
		case MIGRATION_TASK_LOCATION:
		case MIGRATION_MSG_REQUEST:
		case MIGRATION_STACK:
		case MIGRATION_DATA_BSS:

			need_scheduling = handle_migration(p);

			break;
#endif

	/*case NEW_CS_CTP:

		add_ctp(p->producer_task, p->consumer_task, p->dmni_op, p->cs_net);

		break;*/

	case CLEAR_CS_CTP:

		remove_ctp(p->cs_net, p->dmni_op);

		break;

	case SERVICE_TASK_MESSAGE:
	case TASK_TERMINATED:

		//puts("\nAPI\n");

		tcb_ptr = searchTCB(p->consumer_task); //Task ID 0 is the hadlock service ID

		if (tcb_ptr == 0){
			putsv("ERROR: tcb at SERVICE_TASK_MESSAGE_DELIVERY is null: ", p->consumer_task);
			while(1);
		}
		if (tcb_ptr->is_service_task == 0){
			puts("ERROR: Task at SERVICE_TASK_MESSAGE_DELIVERY is not a service task\n");
			while(1);
		}
		if (tcb_ptr->scheduling_ptr->waiting_msg == 0){
			putsv("ERROR: Task at SERVICE_TASK_MESSAGE_DELIVERY is not waiting message: ", (unsigned int)tcb_ptr);
			while(1);
		}

		tgt_message_addr = (unsigned int *) (tcb_ptr->offset | tcb_ptr->reg[3]);

		DMNI_read_data( (unsigned int) tgt_message_addr, p->msg_lenght);

		//Unlock the blocked task
		tcb_ptr->reg[0] = TRUE;

		//Set to ready to execute into scheduler
		tcb_ptr->scheduling_ptr->waiting_msg = 0;
		//puts("Task not waitining anymeore 2\n");

		need_scheduling = 1;

		break;

	case SET_NOC_SWITCHING_CONSUMER:

		//puts("===== CLEAR_CS_TASK\n");

		tcb_ptr = searchTCB(p->consumer_task);

		if (p->cs_mode)  //Stablish Circuit-Switching

			tcb_ptr->add_ctp = p->cs_net << 16 | p->producer_task;

		else  //Stablish Packet-Switching

			tcb_ptr->remove_ctp = p->producer_task;


		//Used globally
		ctp_producer_adress = p->allocated_processor;

		//Test if the task is not waiting for a message
		if (!tcb_ptr->scheduling_ptr->waiting_msg){

			if (tcb_ptr->add_ctp)
				add_ctp_online(tcb_ptr);
			else if (tcb_ptr->remove_ctp)
				remove_ctp_online(tcb_ptr);
		}

		break;

	case SET_NOC_SWITCHING_PRODUCER:

		//puts("++++++ CTP_CLEAR_TO_PRODUCER\n");

		if (p->cs_mode) {
			ctp_ptr = add_ctp(p->producer_task, p->consumer_task, DMNI_SEND_OP, p->cs_net);
		} else
			remove_ctp(p->cs_net, DMNI_SEND_OP);


		send_NoC_switching_ack(p->producer_task, p->consumer_task, p->cs_net, p->cs_mode);

		if (p->cs_mode){
			//puts("Ready to go at producer\n");
			ctp_ptr->ready_to_go = 1;
		}

		break;

	case NOC_SWITCHING_PRODUCER_ACK:

		ctp_ptr = get_ctp_ptr(p->cs_net, DMNI_RECEIVE_OP);

		ctp_ptr->ready_to_go = 1;

		//puts("Ready to go at consumer\n");
		//puts("READY TO GO RECEIVED, producer "); puts(itoa(ctp_ptr->producer_task)); putsv(" consumer ", ctp_ptr->consumer_task);

		break;

	default:
		puts("ERROR: service unknown: "); puts(itoh(p->service));
		putsv(" time: ", MemoryRead(TICK_COUNTER));
		break;
	}

	//putsv("End handle packet - need_scheduling: ", need_scheduling);
	return need_scheduling;
}

/**Add the CTP instance from the CTP array structure, redirect the order to remove the CTP to
 * the producer task. This procedure ensure that no message is lost, because the consumer changes CS
 * before the producer.
 * \param task_to_add TCB pointer of the task to allocate the CTP
 */
void add_ctp_online(TCB * task_to_add){

	ServiceHeader * p;
	int producer_task, subnet;

	producer_task = task_to_add->add_ctp & 0xFFFF;
	subnet = task_to_add->add_ctp >> 16;

	add_ctp(producer_task, task_to_add->id, DMNI_RECEIVE_OP, subnet);

	p = get_service_header_slot();

	//p->header = get_task_location(producer_task);
	p->header = ctp_producer_adress;

	p->service = SET_NOC_SWITCHING_PRODUCER;

	p->producer_task = producer_task;

	p->consumer_task = task_to_add->id;

	p->cs_mode = 1;

	p->cs_net = subnet;

	send_packet(p, 0, 0);

	while(is_send_active(PS_SUBNET));

	task_to_add->add_ctp = 0;
}

/**Remove the CTP instance from the CTP array structure, redirect the order to remove the CTP to
 * the producer task. This producere ensure that no message is lost, because the consumer changes CS
 * before the producer.
 * \param task_to_remove TCB pointer of the task to remove the CS
 */
void remove_ctp_online(TCB * task_to_remove){

	ServiceHeader * p;

	//puts("remove_ctp_online\n");

	int subnet = get_subnet(task_to_remove->remove_ctp, task_to_remove->id, DMNI_RECEIVE_OP);

	if (subnet == -1){
		puts("ERROR, subnet -1\n");
		while(1);
	}

	remove_ctp(subnet, DMNI_RECEIVE_OP);

	p = get_service_header_slot();

	//p->header = get_task_location(task_to_remove->remove_ctp);
	p->header = ctp_producer_adress;

	p->service = SET_NOC_SWITCHING_PRODUCER;

	p->producer_task = task_to_remove->remove_ctp;

	p->consumer_task = task_to_remove->id;

	p->cs_mode = 0;

	p->cs_net = subnet;

	send_packet(p, 0, 0);

	task_to_remove->remove_ctp = 0;

}

/** Generic task scheduler call
 */
void Scheduler() {

	Scheduling * scheduled;
	unsigned int scheduler_call_time;
	TCB * last_executed_task;

	scheduler_call_time = MemoryRead(TICK_COUNTER);

	MemoryWrite(SCHEDULING_REPORT, SCHEDULER);

	#if MIGRATION_ENABLED
		if (current->proc_to_migrate != -1 && current->scheduling_ptr->status == RUNNING && current->scheduling_ptr->waiting_msg == 0){
			migrate_dynamic_memory(current);
		}
	#endif

	last_executed_task = current;

	scheduled = LST(scheduler_call_time);

	if (scheduled){

		//This cast is an approach to reduce the scheduler call overhead
		current = (TCB *) scheduled->tcb_ptr;

		//puts("Scheduled\n");
		current->computation_time = MemoryRead(TICK_COUNTER);

		MemoryWrite(SCHEDULING_REPORT, current->id);

	} else {

		current = &idle_tcb;	// schedules the idle task

		last_idle_time = MemoryRead(TICK_COUNTER);

        MemoryWrite(SCHEDULING_REPORT, IDLE);

#if ENABLE_APL_SLAVE
        if(abs(last_idle_time-last_task_profiling_update) > APL_WINDOW){
        	send_learned_profile(last_idle_time-last_task_profiling_update);
			last_task_profiling_update = last_idle_time;
		}
#endif
	}

	MemoryWrite(TIME_SLICE, get_time_slice() );

	OS_InterruptMaskSet(IRQ_SCHEDULER);

}

/** Function called by assembly (into interruption handler). Implements the routine to handle interruption in HeMPS
 * \param status Status of the interruption. Signal the interruption type
 */
void OS_InterruptServiceRoutine(unsigned int status) {

	volatile ServiceHeader p;
	ServiceHeader * next_service;
	CTP * ctp_ptr;
	unsigned int call_scheduler, subnet, req_status;
	volatile unsigned int ii = 0xF0DA;

	MemoryWrite(SCHEDULING_REPORT, INTERRUPTION);

	if (current == &idle_tcb){
		total_slack_time += MemoryRead(TICK_COUNTER) - last_idle_time;
	} else {
		//current->computation_time = MemoryRead(TICK_COUNTER) - current->computation_time;
		current->total_comp += MemoryRead(TICK_COUNTER) - current->computation_time;
		//puts("["); puts(itoa(current->id)); puts("] Comp --> "); puts(itoa(current->computation_time)); puts("\n");

	}

	call_scheduler = 0;

	if ( status >= IRQ_INIT_NOC ){ //If the interruption comes from the MPN NoC

		if (status & IRQ_PS){//If the interruption comes from the PS subnet

			read_packet((ServiceHeader *)&p);

			if (is_send_active(PS_SUBNET) && (p.service == MESSAGE_REQUEST || p.service == TASK_MIGRATION) ){

				add_pending_service((ServiceHeader *)&p);

			} else {

				call_scheduler = handle_packet(&p, PS_SUBNET);
			}

		} else { //If the interruption comes from the CS subnet

			//Searches the CS subnet - Only USER DATA is supported
			for(subnet=0; subnet < (SUBNETS_NUMBER-1); subnet++){
				if (status & (IRQ_INIT_NOC << subnet) ){

					p.service = MESSAGE_DELIVERY;

					p.consumer_task = get_ctp_ptr(subnet, DMNI_RECEIVE_OP)->consumer_task;

					call_scheduler = handle_packet(&p, subnet);

					break;
				}
			}
		}


	} else if (status & IRQ_CS_REQUEST){//If the interruption comes request wire

		req_status = MemoryRead(READ_CS_REQUEST);

		for(subnet=0; subnet < (SUBNETS_NUMBER-1); subnet++){
			if (req_status & (1 << subnet) ){

				//putsv("Subnet REQUEST interruption: ", MemoryRead(TICK_COUNTER));
				ctp_ptr = get_ctp_ptr(subnet, DMNI_SEND_OP);

				p.service = MESSAGE_REQUEST;
				p.producer_task = ctp_ptr->producer_task;
				p.consumer_task = ctp_ptr->consumer_task;
				//Talvez, na migracao, sera necessario inserir o processador da consumer dentro de CTP...
				p.requesting_processor = get_task_location(ctp_ptr->consumer_task);


				call_scheduler = handle_packet(&p, subnet);
			}
		}

		MemoryWrite(HANDLE_CS_REQUEST, req_status);


	} else if (status & IRQ_PENDING_SERVICE) {

		next_service = get_next_pending_service();
		if (next_service){
			call_scheduler = handle_packet(next_service, PS_SUBNET);
		}


	} else if (status & IRQ_SLACK_TIME){
		send_slack_time_report();
		MemoryWrite(SLACK_TIME_MONITOR, SLACK_TIME_WINDOW);
	}


	if ( status & IRQ_SCHEDULER ){

		call_scheduler = 1;
	}

	if (call_scheduler){

		Scheduler();

	} else if (current == &idle_tcb){

		last_idle_time = MemoryRead(TICK_COUNTER);

		MemoryWrite(SCHEDULING_REPORT, IDLE);

	} else {
		MemoryWrite(SCHEDULING_REPORT, current->id);

		current->computation_time = MemoryRead(TICK_COUNTER);
	}

	//Disable interruption for the task when it is a service task
	if (current->is_service_task){
		//puts("Service task will execute, interruption disabled\n");
		OS_InterruptMaskClear(0xffffffff);
	}

    /*runs the scheduled task*/
    ASM_RunScheduledTask(current);
}

/** Clear a interruption mask
 * \param Mask Interruption mask clear
 */
unsigned int OS_InterruptMaskClear(unsigned int Mask) {

    unsigned int mask;

    mask = MemoryRead(IRQ_MASK) & ~Mask;
    MemoryWrite(IRQ_MASK, mask);

    return mask;
}

/** Set a interruption mask
 * \param Mask Interruption mask set
 */
unsigned int OS_InterruptMaskSet(unsigned int Mask) {

    unsigned int mask;

    mask = MemoryRead(IRQ_MASK) | Mask;
    MemoryWrite(IRQ_MASK, mask);

    return mask;
}

/** Idle function
 */
void OS_Idle() {
	for (;;){
		MemoryWrite(CLOCK_HOLD, 1);
	}
}

int main(){

	ASM_SetInterruptEnable(FALSE);

	idle_tcb.pc = (unsigned int) &OS_Idle;
	idle_tcb.id = 0;
	idle_tcb.offset = 0;

	total_slack_time = 0;

	last_slack_time_report = 0;

	last_idle_time = MemoryRead(TICK_COUNTER);

	current = &idle_tcb;

	init_packet();

	puts("Initializing PE: "); puts(itoh(net_address)); puts("\n");

	init_ctp();

	init_communication();

	init_task_location();

	init_TCBs();

	/*disable interrupts*/
	OS_InterruptMaskClear(0xffffffff);

	//Set to enable all subnets mask
	interrput_mask = 0;
	for (int i=0; i<SUBNETS_NUMBER; i++){
		interrput_mask |= 1 << (4+i); // 4 is because the interruption bit 0, 1, 3, 4 already has been used
	}

	/*enables timeslice counter and wrapper interrupts*/
	interrput_mask = IRQ_SCHEDULER | IRQ_PENDING_SERVICE | IRQ_SLACK_TIME | IRQ_CS_REQUEST | interrput_mask;
	//OS_InterruptMaskSet(IRQ_SCHEDULER | IRQ_PENDING_SERVICE | IRQ_SLACK_TIME | IRQ_CS_REQUEST | interrput_mask);
	//Slack-time disabled
	OS_InterruptMaskSet(interrput_mask);

	MemoryWrite(SLACK_TIME_MONITOR, SLACK_TIME_WINDOW);

	/*runs the scheduled task*/
	ASM_RunScheduledTask(current);

	return 0;
}
