/*
 * qos_self_adaptation.c
 *
 *  Created on: Apr 21, 2017
 *      Author: ruaro
 */

#include "qos_self_adaptation.h"
#include "cs_configuration.h"
#include "../include/services.h"
#include "../../include/kernel_pkg.h"
#include "../include/plasma.h"
#include "packet.h"
#include "resource_manager.h"
#include "processors.h"
#include "utils.h"

Task * migrating_RT_task;
int	candidate_adaptation_proc = -1;
int total_CTPs = -1;


/******* Adaptation Request FIFO variables****/
AdaptationRequest pending_adpt[MAX_ADPT_REQUEST];	//!<pending services array declaration

unsigned int pending_adpt_first = 0;	//!<first valid array index
unsigned int pending_adpt_last = 0;		//!<last valid array index

unsigned char add_pending_adpt = 0; 	//!<Keeps the last operation: 1 - last operation was add. 0 - last operation was remove


/** Assembles and sends a TASK_MIGRATION packet to a slave kernel
 *  \param task_ID The task ID to be migrated
 *  \param new_proc The new processor address of task_ID
 */
int send_task_migration(int task_ID, int new_proc){


	int old_proc;
	Application * app;

	app = get_application_ptr(task_ID >> 8);

	if (app->status != APP_RUNNING){
		putsvsv("Warning! :: Task migration for task ", task_ID, " refused, is not running app ", (task_ID >> 8));
		return 0;
	}

	ServiceHeader *p = get_service_header_slot();

	old_proc = get_task_location(task_ID);

	p->header = old_proc;

	p->service = TASK_MIGRATION;

	p->task_ID = task_ID;

	p->allocated_processor = new_proc;

	send_packet(p, 0, 0);

	//Update task status
	set_task_migrating(task_ID);

	puts("\nTask migration order of task "); puts(itoa(task_ID)); puts(" to proc(old) = "); puts(itoh(old_proc)); puts(" new proc = "); puts(itoh(new_proc)); puts("\n");

	return 1;

}

/** Test if the task processor have BE tasks sharing CPU, if YES, try to move BE task to another processor
 *
 */
int migrate_BE(Task * task_ptr){

	int task_master_addr, BE_task_id, selected_PE;

	task_master_addr = get_master_address(task_ptr->allocated_proc);

	if (task_master_addr != net_address){
		puts("Migrate BE stopped because the master if foreing master!\n");
		return 0; //Impossible to migrate BE task because they belong to another manager
	}
	//Test if there is a free PE, or a PE with only BE tasks, and remove the BE tasks
	BE_task_id = get_BE_task_id(task_ptr->allocated_proc);

	if (BE_task_id != -1){

		//Search for a free PE inside the cluster
		selected_PE = gets_PE_without_RT_task();

		if (selected_PE != -1){
			//Triggers task migration of the BE task
			send_task_migration(task_ptr->id, selected_PE);

			return 1;
		}

		puts("Migrate BE return 0 - THERE IS NO AVALIABLE PE\n");

		return 0;
	}

	puts("Migrate BE return 0 - THERE IS NO BE task in the processor "); puts(itoh(task_ptr->allocated_proc)); puts("\n");

	return 0;
}

/** Selects a new processors to migrated the penalized RT task
 *\param task_ptr Task pointer of the target task
 */
int migrate_RT(Task * task_ptr){

	static int PE_selection_attempts;
	int first_call = 0;


	if (candidate_adaptation_proc == -1){

		PE_selection_attempts = 0;
		migrating_RT_task = task_ptr;
		total_CTPs = get_CTP_number(migrating_RT_task->id, 1); //total CTPs flag enabled
		first_call = 1;

		putsv ("\nRequesting Task Migration for task ", task_ptr->id);

	} else {
		//Releasing the pre-reserved resource
		page_released(clusterID, candidate_adaptation_proc, task_ptr->id);
	}

	PE_selection_attempts++;

	if (candidate_adaptation_proc == -1) //Only to pass the second argument of funcion diamond_serach equal to zero at first time
		candidate_adaptation_proc = 0;

	//This line will search for all processor looking for a utilization larger than task utilization
	//Computation decision function
	//puts("Calling diamond_search...\n");
	candidate_adaptation_proc = diamond_search(migrating_RT_task->allocated_proc, candidate_adaptation_proc, migrating_RT_task->utilization);

	if (candidate_adaptation_proc == -1 || PE_selection_attempts == 3){
		candidate_adaptation_proc = -1;
		cancel_CS_reconfiguration();
		puts("No avaliable processor after diamond serach\n");
		return 0;
	}

	//Pre-reserve the resource
	page_used(clusterID, candidate_adaptation_proc, task_ptr->id);

	//puts("----> Candidade proc selected = "); puts(itoh(candidate_adaptation_proc)); puts("\nCalling CS_reconfiguration (GENERATE_MODE)\n");

	//Starts the process to release CS, when this process finishes, this funcion will call the request_NI_CS
	//The request will be handle by the funcion of this file named: handle_NI_CS_status, that can decide proceed to the adaptation process
	if (first_call){//Only call this funcion at the fist moment
		//puts("\n---- Starting new CS release ----\n");
		release_CS_before_migration(migrating_RT_task->id, candidate_adaptation_proc);

	} else { //Otherwise the CS was already released and the next step is to investigate the CS_NI
		puts("\nAsk again for NI status check..\n");
		CS_Controller_NI_status_check(candidate_adaptation_proc);
	}

	return 1;
}

/** This function evaluates if the selected processor has sufficient free connections
 * This is a communication decision function
 */
void handle_NI_CS_status(int available_connection_in, int available_connection_out){

	int total_in, total_out;

	total_in = total_CTPs >> 8;
	total_out = total_CTPs & 0xFF;

	if (candidate_adaptation_proc == -1){
		puts("ERROR DETECTED\n");
		return;
	}

	puts("\nNI_CS response available: in[");puts(itoa(available_connection_in)); puts("] out[");
	puts(itoa(available_connection_out)); puts("], required: in["); puts(itoa(total_in)); puts("] out[");
	puts(itoa(total_out)); puts("]\n");

	//Tests if the new processor has at the list the same number of avaliable connections
	/*if (available_connection_in < total_in || available_connection_out < total_out ) {
		//Fires a new attempts to migrate the task to another processor
		puts("\nACK reiceved of NI CS - candidate "); puts(itoh(candidate_adaptation_proc)); puts(" NOT have CS support\n");
		migrate_RT(migrating_RT_task);

	} else {*/

		puts("\nACK reiceved of NI CS - candidate "); puts(itoh(candidate_adaptation_proc)); puts(" have full CS support\n");
		//At this point the number of CS avaliable at the candidate proc meet with the connection number of
		//task. The next step if fires the task migration to the candidate proc
		send_task_migration(migrating_RT_task->id, candidate_adaptation_proc);

		//Resets the candidate proc
		candidate_adaptation_proc = -1;
	//}

}



//********************************* Adaptation Request FIFO *************************
/**Add a new pending service. A pending service is a incoming service that cannot be handled immediately by kernel
 * \param pending_service Incoming ServiceHeader pointer
 * \return 1 if add was OK, 0 is the array is full (ERROR situation, while forever)
 */
int add_adaptation_request(Task * task, CS_path * ctp, int type){

	AdaptationRequest * fifo_free_position, * aux_req;
	int i, refuse = 0;

	//puts("\nAdaptation Request added\n");
	if(type == TaskMigration){

		//Test if there is no other TM request with the same task ID or at same Proc

		i = pending_adpt_first;
		while(i != pending_adpt_last){

			aux_req = &pending_adpt[i];

			if (aux_req->type == TaskMigration && (aux_req->task->id == task->id || aux_req->task->allocated_proc == task->allocated_proc)){
				puts("\nTask Migration request REFUSED!!!\n");
				refuse = 1;
				break;
			}

			if (i == MAX_ADPT_REQUEST-1)
				i=0;
			else
				i++;
		}

		if (refuse)
			return 1;

		task->pending_TM = 1;

		//puts("Task Migration to task id "); puts(itoa(task->id)); puts("\n");
	} else if (type == CircuitSwitching){


		i = pending_adpt_first;
		while(i != pending_adpt_last){

			aux_req = &pending_adpt[i];

			if (aux_req->type == CircuitSwitching && aux_req->task == task && aux_req->ctp == ctp){
				puts("\nCircuit Switching request REFUSED!!!\n");
				refuse = 1;
				break;
			}

			if (i == MAX_ADPT_REQUEST-1)
				i=0;
			else
				i++;
		}

		if (refuse)
			return 1;

		ctp->CS_path = CS_PENDING;

		//puts("Circuit-Switching between "); puts(itoa(task->id)); putsv(" -> ", ctp->id);
	} else {
		//puts("ERROR\n");
	}

	//Test if the buffer is full
	if (pending_adpt_first == pending_adpt_last && add_pending_adpt == 1){
		puts("ERROR: AdaptationRequest FIFO FULL\n");
		while(1);
	}

	fifo_free_position = &pending_adpt[pending_adpt_last];

	fifo_free_position->task = task;
	fifo_free_position->type = type;
	fifo_free_position->ctp = ctp;

	if (pending_adpt_last == MAX_ADPT_REQUEST-1){
		pending_adpt_last = 0;
	} else {
		pending_adpt_last++;
	}

	add_pending_adpt = 1;

	return 1;
}

/**Searches by the next pending service, remove then, and return
 * \return The removed ServiceHeader pointer. As it is a memory position, the received part will do something with this pointer and discard the
 * reference
 */
AdaptationRequest * get_next_adapt_request(){

	AdaptationRequest *adapt_request_to_ret;

	//Test if the buffer is empty
	if (pending_adpt_first == pending_adpt_last && add_pending_adpt == 0)
		return 0;

	adapt_request_to_ret = &pending_adpt[pending_adpt_first];

	if (pending_adpt_first == MAX_ADPT_REQUEST-1){
		pending_adpt_first = 0;
	} else {
		pending_adpt_first++;
	}

	add_pending_adpt = 0;

	//puts("\nAdaptation Request removed\n");
	if(adapt_request_to_ret->type == TaskMigration){
		//puts("Task Migration to task id "); puts(itoa(adapt_request_to_ret->task->id)); puts("\n");

		if (adapt_request_to_ret->task->pending_TM == 0){
			//puts("OPS, pending_TM alterado\n");
			return 0;
		}

		adapt_request_to_ret->task->pending_TM = 0;

	} else if (adapt_request_to_ret->type == CircuitSwitching){

		if (adapt_request_to_ret->ctp->CS_path != CS_PENDING){
			//puts("OPS, CS status alterado\n");
			return 0;
		}

		//puts("Circuit-Switching between "); puts(itoa(adapt_request_to_ret->task->id)); putsv(" -> ", adapt_request_to_ret->ctp->id);

	}

	return adapt_request_to_ret;
}

void init_QoS_adaptation(){
	for(int i=0; i<MAX_ADPT_REQUEST; i++){
		pending_adpt[i].task = 0;
		pending_adpt[i].type = -1;
	}
}
