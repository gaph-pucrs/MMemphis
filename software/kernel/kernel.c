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
 * kernel_slave is the core of the OS running into processors.
 * Its job is to runs the user's task.
 * The kernel_slave file uses several modules that implement specific functions
 */

#include "../hal/mips/HAL_kernel.h"
#include "../include/api.h"
#include "../include/management_api.h"
#include "../include/services.h"
#include "modules/packet.h"
#include "modules/pending_service.h"
#include "modules/task_communication.h"
#include "modules/task_scheduler.h"
#include "modules/utils.h"
#include "modules/enforcer_mapping.h"
#include "modules/enforcer_sdn.h"
#include "modules/monitor.h"
#if MIGRATION_ENABLED
#include "modules/enforcer_migration.h"
#endif

//Globals
unsigned int 	interrput_mask;				//!< Stores the bit set allowing interruption, this variable is set within main()
TCB 			idle_tcb;					//!< TCB used to run idle task
TCB *			current;					//!< TCB pointer used to store the current task executing into processor (referenced at assembly - HAL_kernel_asm)


/** Syscall handler. It is called when a task calls a function defined into the api.h file
 * \param service Service of the syscall
 * \param arg0 Generic argument
 * \param arg1 Generic argument
 * \param arg2 Generic argument
 */
int OS_syscall_handler(unsigned int service, unsigned int arg0, unsigned int arg1, unsigned int arg2) {

	unsigned int * msg_address_src;//, * msg_address_tgt;
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

			return send_MA(current, arg1, arg2, arg0);

		case READSERVICE:

			//puts("\nREADSERVICE\n");
			receive_MA(current, arg0);

			//Allows interruptions
			HAL_interrupt_mask_set(interrput_mask);

			return 0;

		case PUTS:

			puts((char *)((current->offset) | (unsigned int) arg0));

			return 1;


/*Here starts the support for the management task API used for MA task*/
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

		case GETTASKLOCATION:

			return get_task_location(arg0);

		case SETTASKRELEASE:

			set_task_release((current->offset | arg0), 0);

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

	int need_scheduling = 0;

	//puts("New Handle packet: "); puts(itoh(p->service)); puts("\n");

	switch (p->service) {

	case MESSAGE_REQUEST:

		need_scheduling = handle_message_request(p);

		break;

	case  MESSAGE_DELIVERY:

		need_scheduling = handle_message_delivery(p, subnet);

		if (current == &idle_tcb){
			need_scheduling = 1;
		}

		break;

	case TASK_ALLOCATION:

		handle_task_allocation(p);

		if (current == &idle_tcb){
			need_scheduling = 1;
		}

		break;

	case TASK_RELEASE:

		set_task_release((unsigned int)p, 1); //last arg equal to 1 means that the data source comes from NoC

		if (current == &idle_tcb){
			need_scheduling = 1;
		}

		break;

	case UPDATE_TASK_LOCATION:

		if (is_another_task_running(p->task_ID >> 8) ){

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

		init_profiling_window();

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

/*These services concerns to the MA task, thus they are bypassed to the respective task using the function handle_MA_message
 *These services could be visible only at MA task scope, however, they still here to allow the debugging to work properly*/
	case TASK_TERMINATED:
	case TASK_ALLOCATED:
	case APP_TERMINATED:
	case TASK_TERMINATED_OTHER_CLUSTER:
	case APP_ALLOCATED:
	case NEW_APP_REQ:
	case NEW_APP:
	case INIT_I_AM_ALIVE:
	case INITIALIZE_MA_TASK:
	case LOAN_PROCESSOR_REQUEST:
	case LOAN_PROCESSOR_DELIVERY:
	case LOAN_PROCESSOR_RELEASE:

	case DETAILED_ROUTING_REQUEST:
	case DETAILED_ROUTING_RESPONSE:
	case TOKEN_REQUEST:
	case TOKEN_RELEASE:
	case TOKEN_GRANT:
	case UPDATE_BORDER_REQUEST:
	case UPDATE_BORDER_ACK:
	case LOCAL_RELEASE_REQUEST:
	case LOCAL_RELEASE_ACK:
	case GLOBAL_MODE_RELEASE:
	case GLOBAL_MODE_RELEASE_ACK:
	case PATH_CONNECTION_REQUEST:
	case PATH_CONNECTION_RELEASE:
	case PATH_CONNECTION_ACK:
	case NI_STATUS_REQUEST:
	case NI_STATUS_RESPONSE:

		handle_MA_message(p->consumer_task, p->msg_lenght);

		need_scheduling = 1;

		break;

	case CLEAR_CS_CTP:

		remove_ctp(p->cs_net, p->dmni_op);

		break;

	case SET_NOC_SWITCHING_CONSUMER:
	case SET_NOC_SWITCHING_PRODUCER:
	case NOC_SWITCHING_PRODUCER_ACK:

		handle_dynamic_CS_setup(p);

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

		reset_last_idle_time();

        HAL_set_scheduling_report(IDLE);

#if ENABLE_APL_SLAVE
        check_profiling_need();
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
		compute_idle_time();
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

		reset_last_idle_time();

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

	reset_last_idle_time();

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
