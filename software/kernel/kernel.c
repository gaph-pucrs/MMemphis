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

#include "kernel.h"
#include "../include/api.h"
#include "../include/management_api.h"
#include "../include/services.h"
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
unsigned int 	interrput_mask;				//!< Stores the bit set allowing interruption, this variable is set within main()
unsigned int 	last_idle_time;				//!< Store the last idle time duration
unsigned int 	last_slack_time_report;		//!< Store time at the last slack time update sent to manager
unsigned int 	total_slack_time;			//!< Store the total of the processor idle time
TCB 			idle_tcb;					//!< TCB pointer used to run idle task
TCB *			current;					//!< TCB pointer used to store the current task executing into processor (referenced at assembly - HAL_kernel_asm)


unsigned int 	last_task_profiling_update;	//!< Used to store the last time that the profiling was sent to manager

/*Function skeletons*/
void send_service_api_message(int, unsigned int, unsigned int *, unsigned int);



int write_local_service_to_task(unsigned int consumer_id, unsigned int * msg_data, int msg_lenght){

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

/** Assembles and sends a TASK_TERMINATED packet to the master kernel
 *  \param terminated_task Terminated task TCB pointer
 */
//void send_task_terminated(TCB * terminated_task, int perc){/*<--- perc**apagar trecho de end simulation****/
void send_task_terminated(TCB * terminated_task){

	unsigned int message[3];
	unsigned int master_addr, master_id;

	message[0] = TASK_TERMINATED;
	message[1] = terminated_task->id; //p->task_ID
	message[2] = terminated_task->master_address;

	master_addr = terminated_task->master_address & 0xFFFF;
	master_id = terminated_task->master_address >> 16;

	if (master_addr == net_address){

		write_local_service_to_task(master_id, message, 3);

	} else {

		send_service_api_message(master_id, master_addr, message, 3);


		puts("Sent task TERMINATED to "); puts(itoh(master_addr)); puts("\n");
		putsv("Master id: ", master_id);

		while(HAL_is_send_active(PS_SUBNET));
	}
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
	unsigned int message[2];

	//Gets the task ID
	master_task_id = allocated_task->master_address >> 16;
	//Gets the task localion
	master_addr = allocated_task->master_address & 0xFFFF;

	message[0] = TASK_ALLOCATED;
	message[1] = allocated_task->id;

	if (master_addr == net_address){

		puts("WRITE TASK ALLOCATED local\n");
		write_local_service_to_task(master_task_id, message, 2);

	} else {

		send_service_api_message(master_task_id, master_addr, message, 2);

		while(HAL_is_send_active(PS_SUBNET));

		puts("Sending task allocated\n");
	}

}


/** Assembles and sends a SLACK_TIME_REPORT packet to the master kernel
 */
void send_slack_time_report(){

	return;

	unsigned int time_aux;

	ServiceHeader * p = get_service_header_slot();

	p->header = cluster_master_address;

	p->service = SLACK_TIME_REPORT;

	time_aux = HAL_get_tick();

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


void send_service_api_message(int consumer_task, unsigned int targetPE, unsigned int * src_message, unsigned int msg_size){

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

		while (HAL_is_receive_active((1 << PS_SUBNET)));

	}
#if CS_DEBUG
	puts("\n********* Send config router ********* \n");
	puts("target_proc = "); puts(itoh(target_proc));
	puts("\ninput port = "); puts(itoa(input_port));
	puts("\noutput_port = "); puts(itoa(output_port));
	puts("\ncs_net = "); puts(itoa(cs_net)); puts("\n***********************\n");
#endif
}


/** Syscall handler. It is called when a task calls a function defined into the api.h file
 * \param service Service of the syscall
 * \param arg0 Generic argument
 * \param arg1 Generic argument
 * \param arg2 Generic argument
 */
int OS_syscall_handler(unsigned int service, unsigned int arg0, unsigned int arg1, unsigned int arg2) {

	TCB * tcb_ptr;
	unsigned int * msg_address_src;//, * msg_address_tgt;
	int consumer_task;
	int producer_task;
	int consumer_PE;
	int appID;


	HAL_disable_scheduler_after_syscall();

	switch (service) {

		case EXIT:

			HAL_enable_scheduler_after_syscall();

			//Deadlock avoidance: avoids to send a packet when the DMNI is busy in send process
			if (HAL_is_send_active(PS_SUBNET)){
				return 0;
			}

			puts("Task id: "); puts(itoa(current->id)); putsv(" terminated at ", HAL_get_tick());

			//send_task_terminated(current, arg0);
			send_task_terminated(current);

			clear_scheduling(current->scheduling_ptr);

			appID = current->id >> 8;

			if ( !is_another_task_running(appID) ){

				clear_app_tasks_locations(appID);
			}

		return 1;

		case SENDMESSAGE:

			return send_message(current, arg0, arg1);

		case RECVMESSAGE:

			return receive_message(current, arg0, arg1);

		case GETTICK:

			return HAL_get_tick();

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

			if (HAL_is_send_active(PS_SUBNET)){
				return 0;
			}

			//putsv("\nReal-time to task: ", current->id);

			real_time_task(current->scheduling_ptr, arg0, arg1, arg2);

			send_real_time_constraints(current->scheduling_ptr);

			return 1;

		break;

		case SENDRAW:

			HAL_enable_scheduler_after_syscall();

			if(HAL_is_send_active(PS_SUBNET)){
				puts("Preso no SENDRAW\n");
				return 0;
			}


			msg_address_src = (unsigned int *) (current->offset | arg0);

			DMNI_send_data((unsigned int)msg_address_src, arg1, PS_SUBNET);

			return 1;

		/************************* SERVICE API FUNCTIONS **************************/
		case REQSERVICEMODE:
			//TODO: a protocol that only grants a service permission to secure tasks
			current->is_service_task = 1;

			puts("Task REQUESTED service - disabling interruption\n");
			HAL_interrupt_mask_clear(0xffffffff);

			return 1;

		case WRITESERVICE:

			//Enable to schedule after syscall and interruptions since task will be blocked anyway
			HAL_enable_scheduler_after_syscall();

			if (HAL_is_send_active(PS_SUBNET)){
				puts("Preso no WRITESERVICE\n");
				return 0;
			}

			//Test if the task has permission to use service API
			if (!current->is_service_task){
				puts("Task is not a service task YET\n");
				return 0;
			}

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

				//puts("Local send service from task "); puts(itoa(producer_task)); putsv(" to task ", consumer_task);

				//If target is not waiting message return 0 and schedules target to it consume the message
				if (!write_local_service_to_task(consumer_task, msg_address_src, arg2)){
					HAL_enable_scheduler_after_syscall();
					return 0;
				}

			} else { //Else send the message to the remote task

				//if (HAL_is_send_active(PS_SUBNET))
				//	return 0;
				send_service_api_message(consumer_task, consumer_PE, msg_address_src, arg2);
			}

			return 1;

		case READSERVICE:

			//puts("\nREADSERVICE\n");

			HAL_enable_scheduler_after_syscall();

			if (!current->is_service_task)
				return 0;

			//consumer_task =  current->id;
			//producer_task = (int) arg1;

			/*putsv("Address passed: ", arg0);
			putsv("Full address: ", (current->offset) | arg0);
			putsv("Offset: ", (current->offset));*/

			current->scheduling_ptr->waiting_msg = 1;
			//putsv("Task is waiting message: ", (unsigned int)current);

			HAL_interrupt_mask_set(interrput_mask);

			return 0;

		case PUTS:

			puts((char *)((current->offset) | (unsigned int) arg0));

			return 1;

		case CFGROUTER:

			if (HAL_is_send_active(PS_SUBNET))
				return 0;

			send_config_router(arg0, arg1 >> 8, arg1 & 0xFF, arg2);

			return 1;
		case NOCSENDFREE:
			//If DMNI send is active them return FALSE
			if (HAL_is_send_active(PS_SUBNET))
				return 0;
			//Otherwise return TRUE
			return 1;

		case INCOMINGPACKET:
			return (HAL_get_irq_status() >= IRQ_INIT_NOC);

		case GETNETADDRESS:
			return net_address;
		case ADDTASKLOCATION:
			add_task_location(arg0, arg1);
			//puts("Added task id "); puts(itoa(arg0)); puts(" at loc "); puts(itoh(arg1)); puts("\n");
			break;
		case REMOVETASKLOCATION:
			remove_task_location(arg0);
			break;
		case GETTASKLOCATION:
			return get_task_location(arg0);
		case SETMYID:
			current->id = arg0;
			//putsv("Task id changed to: ", current->id);
			break;
		case SETTASLRELEASE:

			msg_address_src = (unsigned int *) (current->offset | arg0);
			//msg_size = arg1

			consumer_task = msg_address_src[3];

			putsv("TASK_RELEASE\nTask ID: ", consumer_task);

			tcb_ptr = searchTCB(consumer_task);

			appID = consumer_task >> 8;

			tcb_ptr->data_lenght = msg_address_src[9];

			puts("Data lenght: "); puts(itoh(tcb_ptr->data_lenght)); puts("\n");

			tcb_ptr->bss_lenght = msg_address_src[11];

			puts("BSS lenght: "); puts(itoh(tcb_ptr->data_lenght)); puts("\n");

			tcb_ptr->text_lenght = tcb_ptr->text_lenght - tcb_ptr->data_lenght;

			if (tcb_ptr->scheduling_ptr->status == BLOCKED){
				tcb_ptr->scheduling_ptr->status = READY;
			}

			putsv("app task number: ", msg_address_src[8]);

			for(int i=0; i<msg_address_src[8]; i++){
				add_task_location(appID << 8 | i, msg_address_src[CONSTANT_PKT_SIZE+i]);
				puts("Add task "); puts(itoa(appID << 8 | i)); puts(" loc "); puts(itoh(msg_address_src[CONSTANT_PKT_SIZE+i])); puts("\n");
			}

			break;
		default:
			putsv("Syscall code unknow: ", service);
			while(1);
			break;
	}

	return 0;
}

/** Handles a new packet from NoC
 */
int handle_packet(volatile ServiceHeader * p, unsigned int subnet) {

	int need_scheduling, code_lenght, app_ID, task_loc;
	unsigned int app_tasks_location[MAX_TASKS_APP];
	unsigned int * tgt_message_addr;
	TCB * tcb_ptr = 0;
	CTP * ctp_ptr = 0;

	need_scheduling = 0;

	//puts("New Handle packet: "); puts(itoh(p->service)); puts("\n");

	switch (p->service) {

	case MESSAGE_REQUEST: //This case is the most complicated of the HeMPS if you understand it, so you understand all task communication protocol
	//This case sincronizes the communication messages also in case of task migration, the migration allows several scenarios, that are handled inside this case

		need_scheduling = handle_message_request(p);

		break;

	case  MESSAGE_DELIVERY:

		need_scheduling = handle_message_delivery(p, subnet);

		if (current == &idle_tcb){
			need_scheduling = 1;
		}

		break;

	case TASK_ALLOCATION:

		tcb_ptr = search_free_TCB();

		tcb_ptr->pc = 0;

		tcb_ptr->id = p->task_ID;

		puts("Task id: "); puts(itoa(tcb_ptr->id)); putsv(" allocated at ", HAL_get_tick());

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

		if(tcb_ptr->id == 1026)
			putsv("TASK_RELEASE\nTask ID: ", p->task_ID);

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

		/*if (is_disturbing_prod_PE()){
			disturbing_generator();
		} else if (is_disturbing_cons_PE()){
			disturbing_reader();
		}*/

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

	case TASK_TERMINATED:
	case TASK_ALLOCATED:
	case APP_TERMINATED:
	case TASK_TERMINATED_OTHER_CLUSTER:
	case APP_ALLOCATED:
	case NEW_APP_REQ:
	case NEW_APP:
	case SERVICE_TASK_MESSAGE:
	case INIT_I_AM_ALIVE:
	case INIT_LOCAL:
	case LOAN_PROCESSOR_REQUEST:
	case LOAN_PROCESSOR_DELIVERY:
	case LOAN_PROCESSOR_RELEASE:

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
			putsv("ERROR: Task at SERVICE_TASK_MESSAGE_DELIVERY is not waiting message: ", tcb_ptr->id);
			while(1);
		}

		tgt_message_addr = (unsigned int *) (tcb_ptr->offset | tcb_ptr->reg[3]);

		DMNI_read_data( (unsigned int) tgt_message_addr, p->msg_lenght);

		HAL_release_waiting_task(tcb_ptr);

		need_scheduling = 1;

		break;

	case SET_NOC_SWITCHING_CONSUMER:

		//puts("===== CLEAR_CS_TASK\n");

		tcb_ptr = searchTCB(p->consumer_task);

		if (p->cs_mode)  //Stablish Circuit-Switching

			tcb_ptr->add_ctp = p->cs_net << 16 | p->producer_task;

		else  //Stablish Packet-Switching

			tcb_ptr->remove_ctp = p->producer_task;


		set_ctp_producer_adress(p->allocated_processor);

		//Test if the task is not waiting for a message
		if (!tcb_ptr->scheduling_ptr->waiting_msg){

			check_ctp_reconfiguration(tcb_ptr);
		}

		break;

	case SET_NOC_SWITCHING_PRODUCER:

		puts("++++++ CTP_CLEAR_TO_PRODUCER\n");

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
		putsv(" time: ", HAL_get_tick());
		break;
	}

	//putsv("End handle packet - need_scheduling: ", need_scheduling);
	return need_scheduling;
}


/** Generic task scheduler call
 */
void OS_scheduler() {

	Scheduling * scheduled;
	unsigned int scheduler_call_time;
	TCB * last_executed_task;

	scheduler_call_time = HAL_get_tick();

	HAL_set_scheduling_report(SCHEDULER);

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
		current->computation_time = HAL_get_tick();

		HAL_set_scheduling_report(current->id);

	} else {

		current = &idle_tcb;	// schedules the idle task

		last_idle_time = HAL_get_tick();

        HAL_set_scheduling_report(IDLE);

#if ENABLE_APL_SLAVE
        if(abs(last_idle_time-last_task_profiling_update) > APL_WINDOW){
        	send_learned_profile(last_idle_time-last_task_profiling_update);
			last_task_profiling_update = last_idle_time;
		}
#endif
	}

	HAL_set_time_slice(get_time_slice());

	HAL_interrupt_mask_set(IRQ_SCHEDULER);

	//Disable interruption for the task when it is a service task
	if (current->is_service_task){
		//puts("Service task will execute, interruption disabled\n");
		HAL_interrupt_mask_clear(0xffffffff);
	}

}

/** Function called by assembly (into interruption handler). Implements the routine to handle interruption in HeMPS
 * \param status Status of the interruption. Signal the interruption type
 */
void OS_interruption_handler(unsigned int status) {

	volatile ServiceHeader p;
	ServiceHeader * next_service;
	CTP * ctp_ptr;
	unsigned int call_scheduler, subnet, req_status;

	HAL_set_scheduling_report(INTERRUPTION);

	if (current == &idle_tcb){
		total_slack_time += HAL_get_tick() - last_idle_time;
	} else {
		//current->computation_time = HAL_get_tick() - current->computation_time;
		current->total_comp += HAL_get_tick() - current->computation_time;
		//puts("["); puts(itoa(current->id)); puts("] Comp --> "); puts(itoa(current->computation_time)); puts("\n");

	}

	call_scheduler = 0;

	if ( status >= IRQ_INIT_NOC ){ //If the interruption comes from the MPN NoC

		if (status & IRQ_PS){//If the interruption comes from the PS subnet

			read_packet((ServiceHeader *)&p);

			if (HAL_is_send_active(PS_SUBNET) && (p.service == MESSAGE_REQUEST || p.service == TASK_MIGRATION) ){

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

		req_status = HAL_get_CS_request();

		for(subnet=0; subnet < (SUBNETS_NUMBER-1); subnet++){
			if (req_status & (1 << subnet) ){

				//putsv("Subnet REQUEST interruption: ", HAL_get_tick());
				ctp_ptr = get_ctp_ptr(subnet, DMNI_SEND_OP);

				p.service = MESSAGE_REQUEST;
				p.producer_task = ctp_ptr->producer_task;
				p.consumer_task = ctp_ptr->consumer_task;
				//Talvez, na migracao, sera necessario inserir o processador da consumer dentro de CTP...
				p.requesting_processor = get_task_location(ctp_ptr->consumer_task);


				call_scheduler = handle_packet(&p, subnet);
			}
		}

		HAL_handle_CS_request(req_status);


	} else if (status & IRQ_PENDING_SERVICE) {

		next_service = get_next_pending_service();
		if (next_service){
			call_scheduler = handle_packet(next_service, PS_SUBNET);
		}


	} else if (status & IRQ_SLACK_TIME){
		send_slack_time_report();
		HAL_set_slack_time_monitor(SLACK_TIME_WINDOW);
	}


	if ( status & IRQ_SCHEDULER ){

		call_scheduler = 1;
	}

	if (call_scheduler){

		OS_scheduler();

	} else if (current == &idle_tcb){

		last_idle_time = HAL_get_tick();

		HAL_set_scheduling_report(IDLE);

	} else {
		HAL_set_scheduling_report(current->id);

		current->computation_time = HAL_get_tick();
	}

	//Disable interruption for the task when it is a service task
	/*if (current->is_service_task){
		//puts("Service task will execute, interruption disabled\n");
		puts("INT disabled\n");
		HAL_interrupt_mask_clear(0xffffffff);
	}*/

    /*runs the scheduled task*/
    HAL_run_scheduled_task((unsigned int) current);
}



/** Idle function
 */
void OS_Idle() {
	for (;;){
		HAL_set_clock_hold(1);
	}
}

int main(){

	HAL_set_interrupt_enabled(0); //Set interruption disabled

	idle_tcb.pc = (unsigned int) &OS_Idle;
	idle_tcb.id = 0;
	idle_tcb.offset = 0;

	total_slack_time = 0;

	last_slack_time_report = 0;

	last_idle_time = HAL_get_tick();

	current = &idle_tcb;

	init_HAL();

	init_packet();

	puts("Initializing PE: "); puts(itoh(net_address)); puts("\n");

	init_ctp();

	init_communication();

	init_task_location();

	init_TCBs();

	/*disable interrupts*/
	HAL_interrupt_mask_clear(0xffffffff);

	//Set to enable all subnets mask
	interrput_mask = 0;
	for (int i=0; i<SUBNETS_NUMBER; i++){
		interrput_mask |= 1 << (4+i); // 4 is because the interruption bit 0, 1, 3, 4 already has been used
	}

	/*enables timeslice counter and wrapper interrupts*/
	interrput_mask = IRQ_SCHEDULER | IRQ_PENDING_SERVICE | IRQ_SLACK_TIME | IRQ_CS_REQUEST | interrput_mask;
	//OS_InterruptMaskSet(IRQ_SCHEDULER | IRQ_PENDING_SERVICE | IRQ_SLACK_TIME | IRQ_CS_REQUEST | interrput_mask);
	//Slack-time disabled
	HAL_interrupt_mask_set(interrput_mask);

	HAL_set_slack_time_monitor(SLACK_TIME_WINDOW);

	/*runs the scheduled task*/
	HAL_run_scheduled_task((unsigned int)current);

	return 0;
}
