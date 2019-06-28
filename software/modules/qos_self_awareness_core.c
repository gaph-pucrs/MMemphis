/*
 * qos_self_awareness_core.c
 *
 *  Created on: Apr 20, 2017
 *      Author: ruaro
 */

#include "qos_self_awareness_core.h"

#include "qos_self_adaptation.h"
#include "qos_learning.h"
#include "applications.h"
#include "task_location.h" //input from task mapping
#include "cs_configuration.h" //input from about the CS paths
#include "processors.h" //input of the slack-time (pre-processing)
#include "resource_manager.h"
#include "../include/plasma.h"
#include "../include/services.h"
#include "utils.h"

#define PERIOD_OVERHAUL		10
#define MAX_LATENCY_MISS	1

void complete_checkup();
void quick_ckeckup_computation();
int computes_task_score(Task *);
void prediction_analysis(Application *);


int		remote_PE_slack_time;	//!< Stores the remote PE slack time percentage
int		remote_PE_utilization;	//!< Stores the remote PE utilization percentage

int		latency_miss_prod_proc;
int		latency_miss_prod_task;
int		latency_miss_cons_proc;
int		latency_miss_cons_task;

Application * app_over_analysis = 0;
Task		* task_over_analysis = 0;

//------------------------------------------------ REMOTE CPU DATA PROTOCOL ------------------------------------

RemoteCPUData remote_CPU_data[MAX_TASKS_APP];
int 		  waiting_remote_packets;

enum checkup_mode {QUICK_CHECKUP, COMPLETE_CHECKUP};
enum checkup_mode current_checkup = QUICK_CHECKUP;

/** Assemble and sends a PE_STATISTICS_REQUEST to another manager
 *  \param rt_task	Pointer to the RT task
 *	\param target_manager_address Address XY of the target manager processor
 */
void send_PE_statistics_request(int task_proc){

	ServiceHeader *p;

	p = get_service_header_slot();

	p->header = get_master_address(task_proc);

	p->service = PE_STATISTICS_REQUEST;

	p->app_ID = app_over_analysis->app_ID;

	p->requesting_processor = task_proc;

	send_packet(p, 0, 0);

	//puts("\nPE statistics REQUEST from proc "); puts(itoh(task_proc)); puts("\n");
}


int get_remote_CPU_utilization(int proc_address){

	for(int i=0; i<MAX_TASKS_APP; i++){
		if (remote_CPU_data[i].proc == proc_address)
			return remote_CPU_data[i].utilization;
	}

	puts("ERROR, no remote proc found in utilization\n");
	while(1);
	return 0;
}

int get_remote_CPU_computation_task_number(int proc_address){

	for(int i=0; i<MAX_TASKS_APP; i++){
		if (remote_CPU_data[i].proc == proc_address)
			return remote_CPU_data[i].computation_task_number;
	}

	puts("ERROR, no remote proc found in slack_time\n");
	while(1);
	return 0;
}

int get_remote_CPU_num_RT_tasks(int proc_address){

	for(int i=0; i<MAX_TASKS_APP; i++){
		if (remote_CPU_data[i].proc == proc_address)
			return remote_CPU_data[i].num_RT_tasks;
	}

	puts("ERROR, no remote proc found in slack_time\n");
	while(1);
	return 0;
}

/** This function is called several times until the data be completely fulfilled
 *
 */
void remote_CPU_data_protocol(int proc_address, int proc_utilization, int comp_task_number, int num_RT_tasks){

	int master_address, task_proc;

	//proc_address == -1 means that the function is called in initialization mode
	if (proc_address == -1){

		//puts("Initilization of remote protocol\n");

		//Clear structure
		for(int i=0; i<MAX_TASKS_APP; i++)
			remote_CPU_data[i].proc = -1;

		waiting_remote_packets = 0;

		if (current_checkup == QUICK_CHECKUP){

			//puts("QUICK_CHECKUP\n");

			task_proc = task_over_analysis->allocated_proc;

			master_address = get_master_address(task_proc);

			if (master_address != net_address){

				remote_CPU_data[waiting_remote_packets].proc = task_proc;

				waiting_remote_packets++;
			}

		} else { //COMPLETE_CHECKUP

			//puts("COMPLETE_CHECKUP\n");

			for(int i=0; i<app_over_analysis->tasks_number; i++){

				task_proc = app_over_analysis->tasks[i].allocated_proc;

				master_address = get_master_address(task_proc);

				if (master_address != net_address){

					remote_CPU_data[waiting_remote_packets].proc = task_proc;

					waiting_remote_packets++;

				}

			} //end for

		}// end else

	} else { //This else is executed when a packet is received, the three parameters are only used inside this else

		waiting_remote_packets--;

		/*puts("PE statistics RESPONSE from proc "); puts(itoh(proc_address));
		puts(" utilization "); puts(itoa(proc_utilization));
		puts(" slack "); puts(itoa(proc_slack_time));
		puts("\nwaiting_remote_packets "); puts(itoa(waiting_remote_packets));
		puts("\n");*/

		if (remote_CPU_data[waiting_remote_packets].proc != proc_address){
			puts("ERROR, diferent proc\n");
			while(1);
		}

		//Fills the structure with remote data
		remote_CPU_data[waiting_remote_packets].utilization = proc_utilization;

		remote_CPU_data[waiting_remote_packets].computation_task_number = comp_task_number;

		remote_CPU_data[waiting_remote_packets].num_RT_tasks = num_RT_tasks;

	}

	if (waiting_remote_packets){

		send_PE_statistics_request(remote_CPU_data[waiting_remote_packets-1].proc);

	} else { //End of remote CPU data protocoll

		//puts("Remote protocol finishes\n");

		//Fires the apropriated checkup and set initialization to 1 preparing to a future protocol
		if (current_checkup == QUICK_CHECKUP)
			quick_ckeckup_computation();
		else
			complete_checkup();
	}

}
//------------------------------------------------ END REMOTE CPU DATA PROTOCOL ------------------------------------

/** Fired by a latency miss
 *
 */
int quick_checkup_communication(int prod_task, int cons_task, int source_proc, int target_proc) {

	CS_path * ctp;

	if (!is_CS_NOT_active()){
		return 0;
	}

	//puts("\n%%%%%%%%%%%%  Starting QUICK_CHECKUP COMMUNICATION %%%%%%%%%%%%%%%%%%\n");
	puts("Starting QUICK_CHECKUP COMMUNICATION:\n");

	ctp = search_CS_path(prod_task, cons_task);

	//If latency miss is very higher
	if (ctp->latency_miss > MAX_LATENCY_MISS*3) {

		//Returns 1 meaning that a complete_checkup is need
		return 1;

	} else if (ctp->latency_miss > MAX_LATENCY_MISS){

		//Try to stablishes CS
		add_adaptation_request(task_over_analysis, ctp, CircuitSwitching);

		clear_latency_and_deadline_miss(task_over_analysis->id);
		clear_latency_and_deadline_miss(ctp->id);
		//request_runtime_CS(prod_task, cons_task, source_proc, target_proc);

	} else
		puts("Latency miss ignored\n");

	app_over_analysis = 0;
	task_over_analysis = 0;

	return 0;

}


/** Fired by a deadline miss or RT change
 * The ideia is perform a quick_checkup with low overhead impact
 */
void quick_ckeckup_computation(){

	int pe_util, deadline_rate, current_time;
	int master_address;

	current_time = MemoryRead(TICK_COUNTER);

	//puts("\n+++++++++++  Starting QUICK_CHECKUP COMPUTATION +++++++++++-\n");
	puts("Starting QUICK_CHECKUP COMPUTATION:\n");

	master_address = get_master_address(task_over_analysis->allocated_proc);

	if (master_address != net_address)
		pe_util = get_remote_CPU_utilization(task_over_analysis->allocated_proc);
	else
		pe_util = get_proc_utilization(task_over_analysis->allocated_proc);


	//1. Check CPU utilization
	if (pe_util > 100) {

		puts("Migrate RT task\n");

		add_adaptation_request(task_over_analysis, 0, TaskMigration);

		clear_latency_and_deadline_miss(task_over_analysis->id);
		//migrate_RT(task_over_analysis);

	//2. Sporadic latency miss
	} else {

		//Computes deadline miss raet
		deadline_rate = task_over_analysis->deadline_miss * 100 / (task_over_analysis->period * PERIOD_OVERHAUL);

		if (deadline_rate >= 1){ //>= 1%

			puts("\n-->>> Deadline rate higher than 1%\n");
			add_adaptation_request(task_over_analysis, 0, TaskMigration);

			clear_latency_and_deadline_miss(task_over_analysis->id);
			//migrate_RT(task_over_analysis);

		}
	}

	app_over_analysis = 0;
	task_over_analysis = 0;
}

//Implementando

/** Evaluates all task of an applications
 * Is fired according two conditions:
 * 	1. At every 10 applications hyper-period
 * 	2. If the last quick_checkup fails to restore QoS for the task
 *
 * 	The complete_checkup also uses the Learned Application Profile (LAP) to act proactively
 *
 * 	1. Evaluates all task score
 * 	2. Choose an action based on the score
 * 	3. If all task are OK
 * 	4. Observe the LAP
 * 	5. Take a proactive action
 */
void complete_checkup(){

	Task * task, * selected_task;
	int max_score, lower_util;
	int task_rank[MAX_TASKS_APP]; //The index is used to store the task ID, the value stores the task status
	//puts("\n>>>>>> Starting COMPLETE_CHECKUP <<<<<<<<\n");
	puts("Starting COMPLETE_CHECKUP:\n");

	//Initializes task rank
	for(int i=0; i<MAX_TASKS_APP; i++)
		task_rank[i] = -1;

	max_score = 0;
	//For each app task gets its rank
	for(int i=0; i<app_over_analysis->tasks_number; i++){

		task = &app_over_analysis->tasks[i];

		task_rank[i] = computes_task_score(task);

		if (max_score < task_rank[i])
			max_score = task_rank[i];

		puts("\tTask "); puts(itoa(task->id)); puts("\t = ");
		puts(itoa(task_rank[i])); puts("\n");
	}

	//If there is a task with score higher than 0
	if (max_score){

		lower_util = 100;
		selected_task = 0;
		for(int i=0; i<app_over_analysis->tasks_number; i++){

			if (max_score == task_rank[i]){

				task = &app_over_analysis->tasks[i];

				if (!task->pending_TM && (!selected_task || lower_util > task->utilization)){
					selected_task = task;
					lower_util = task->utilization;
				}
			}
		}

		if (selected_task){
			putsv("\n Vai migra por complete_ckeckup task ", selected_task->id);
			add_adaptation_request(selected_task, TaskMigration, 0);
			puts("Passou\n");
		}

	} else {
		puts("\nAll task OK!!!\n");
#if ENABLE_QOS_PREDICTION
		prediction_analysis(app_over_analysis);
#endif
	}

	//Clear all deadline and latency miss
	for(int i=0; i<app_over_analysis->tasks_number; i++)
		clear_latency_and_deadline_miss(app_over_analysis->tasks[i].id);

	app_over_analysis = 0;
	task_over_analysis = 0;
}


int computes_task_score(Task * task){

	int pe_util, master_address, latency_miss_sum, pe_rt_number, deadline_count, task_score;

	master_address = get_master_address(task->allocated_proc);

	if (master_address != net_address){
		pe_util = get_remote_CPU_utilization(task->allocated_proc);
		pe_rt_number = get_remote_CPU_num_RT_tasks(task->allocated_proc);
	} else {
		pe_util = get_proc_utilization(task->allocated_proc);
		pe_rt_number = get_RT_task_number(task->allocated_proc);
	}

	task_score = 0;
	latency_miss_sum = 0;
	deadline_count = 0;

	if (pe_util > 100)
		task_score = pe_util;

	latency_miss_sum = get_total_latency_miss(task->id);
	if (latency_miss_sum <= MAX_LATENCY_MISS)
		latency_miss_sum = 0;

	if (task->deadline_miss > 1 && pe_rt_number > 1)//Discard 1 deadline miss
		deadline_count = task->deadline_miss;

	task_score +=  deadline_count + latency_miss_sum;

	return task_score;


	//Defines the task status
	/*if (pe_util > 100){
		puts("CRITICAL utilization\n");
		return CRITICAL;
	} else if (pe_util > 95 && task->utilization < pe_util) {
		puts("ATTENTION utilization\n");
		task_status = ATTENTION;
	}

	if(deadline_miss_rate > 1) {
		puts("CRITICAL deadline\n");
		return CRITICAL;
	} else if (deadline_miss_rate == 1) {
		task_status = ATTENTION;
		puts("ATTENTION deadline\n");
	}

	if (latency_miss_rate > MAX_LATENCY_MISS*3) {
		puts("CRITICAL latency\n");
		return CRITICAL;
	} else if (latency_miss_rate > MAX_LATENCY_MISS) {
		task_status = ATTENTION;
		puts("ATTENTION latency\n");
	}

	return task_status;*/
}


inline int is_QoS_analysis_active(){
	return (app_over_analysis || task_over_analysis);
}


/** Selects the checkup type according to the parameters and the system status
 */
void QoS_analysis(unsigned int routine_overhaul_flag, ServiceHeader *packet){

	int task_id;
	int current_time;

#if ENABLE_QOS_ADAPTATION
#else
	return;
#endif

	if (!is_CS_NOT_active() || app_over_analysis || task_over_analysis){
		//puts("QoS Analises suspeded because CS Reconfiguration is active\n");
		return;
	}

	//puts("\nQoS Analysis:\n");

	if (routine_overhaul_flag) {

		app_over_analysis = (Application *) routine_overhaul_flag;

		current_checkup = COMPLETE_CHECKUP;

		puts("Routine_overhaul_flag for app "); puts(itoa(app_over_analysis->app_ID)); puts("\n");

	} else {

		if (packet->service == LATENCY_MISS_REPORT)
			task_id = packet->producer_task;
		else
			task_id = packet->task_ID;

		//putsv("\tTask id: ", task_id);

		app_over_analysis = get_application_ptr(task_id >> 8);
		task_over_analysis = get_task_ptr(app_over_analysis, task_id);

		switch (packet->service) {

			case RT_CONSTRANTS:

				current_checkup = QUICK_CHECKUP;

				break;

			case DEADLINE_MISS_REPORT:

				//putsv("--! deadline_miss for task ", task_over_analysis->id);

				current_time = MemoryRead(TICK_COUNTER);

				//Test if the last adaptation does not resolved the problem and set the checkup as complete
				if (current_time - task_over_analysis->last_quick_checkup < (task_over_analysis->period * 2)){

					current_checkup = COMPLETE_CHECKUP;

					puts("\t\t#### Second deadline miss whitin 2 periods\n");

				} else {

					current_checkup = QUICK_CHECKUP;

					puts("\t\t~~~ Sporadic deadline miss\n");

					task_over_analysis->last_quick_checkup = current_time;

				}

				break;

			case LATENCY_MISS_REPORT:

				if (quick_checkup_communication(task_id, packet->consumer_task, packet->target_processor, packet->source_PE))

					current_checkup = COMPLETE_CHECKUP;

				else
					//Return directly withou call the remote_CPU_data_protocol function
					return;

				break;

			default:
				break;
		}
	}

	//Fires the remote_CPU_data_protocol only if necessary
	remote_CPU_data_protocol(-1, 0, 0, 0);
}

void check_app_routine_overhaul(){

	Application * app;
	int current_time;

	current_time = MemoryRead(TICK_COUNTER);

	for(int i=0; i<MAX_CLUSTER_APP; i++){

		app = get_next_RT_app();

		if (!app) continue;


		if ( app->hyperperiod && (app->hyperperiod * PERIOD_OVERHAUL) < (current_time - app->last_complete_checkup) ){

			/*puts("\n******************************************\nApplication routine overhaul time "); puts(itoa(current_time));
			puts(" app "); puts(itoa(app->app_ID));
			puts(" hyperperiod "); puts(itoa(app->hyperperiod));
			puts(" last checkup "); puts(itoa(app->last_complete_checkup));
			puts("\n******************************************\n");*/
			QoS_analysis((unsigned int) app, 0);

			app->last_complete_checkup = current_time;

			return;

		}
	}

}

/** Handle the profiling packet from the slave and insert into the database
 * with the communication and computation profiling for all task of the application
 */
void handle_learned_profile_packet(int task_id, int learned_profile){

	Application * app;
	Task * task;

	int comm, comp;

	comp = learned_profile >> 16;
	comm = learned_profile & 0xFFFF;

	app = get_application_ptr(task_id >> 8);
	task = get_task_ptr(app, task_id);

	if (task->communication_profile)
		task->communication_profile = (task->communication_profile + comm) / 2;
	else
		task->communication_profile = comm;

	if (task->computation_profile)
		task->computation_profile = (task->computation_profile + comp) / 2;
	else
		task->computation_profile = comp;

	//if (task_id == 257){

	//puts(itoa(task_id)); puts("\t");puts(itoa(100-(comm+comp))); puts("\t");puts(itoa(comm));puts("\t");puts(itoa(comp)); puts("\n");

	//puts(itoa(task_id)); puts("\t");puts(itoa(comm));puts("\t");puts(itoa(comp)); puts("\t"); puts(itoa(task->communication_profile)); puts("\t"); puts(itoa(task->computation_profile)); puts("\n");

	puts(itoa(task_id)); puts("\t"); puts(itoa(task->computation_profile)); puts("\t"); puts(itoa(task->communication_profile)); puts("\n");

	//}

}

/*
 * Quick checkup
 *
 * 1. Evaluate the obvious, if the task miss a deadline try to migrate the task, if the task miss a latency, try to
 * stablishes CS
 *
 * 2. There have filters, if the task misses several deadlines or several CS this means that the last action do not
 * take effects and the complete_checkup must be called
 *
 */

/*
 * Complete checkup evaluation
 * 1. Is based on deadline and latency miss
 *    a) Rank the tasks
 *
 * 2) If alls task are OK and the application raw for a strongly time
 *    b) Calls the proactive analises, based on APL
 */

/*
 * Task Migration Flags
 * 1. RT utilization 				- avoid PE with higher RT utilization
 * 2. Computational intensive task  - avoid PE with computational intensive task
 * 3. Number of RT task mapped		- avoid PE with a higher number of RT task
 * 4. Number of task mapped			- avoid PE with a higher number of any task
 */

/**
 * 1). Proactive action at computation level
 *
 *  a) Task is computation intensive AND there is other computation intensive task running together
 *     = Search the closest free PE or a PE with idle or communication intensive tasks mapped
 *
 * 2. Proactive action at communication level
 * 	a) Task is communication intensive = stablish CS for it
 *
 *  What is a communication intensive task
 *     That one that spend more than 25% of time communicating
 *
 *  What is a computation intensive task
 *     That one that pend more than 50% of time computating
 *
 *  What is a busy task
 *     That one that is communication and computation intensive
 *
 *  What is a idle task
 *     That one that is neither communication or computation intensive
 *
 *  Flow:
 *  1. Selects all task that are computation intensive and are running in PE with other computation intensive task
 *		a) OBS: when there are 2 computational task, migrate only one
 *  2. A list of proactive_task_migration will be filled.
 *  3. Look for any CTP that both task are not in the proactive_task_migration and the consumer is communication intensive
 *  4. Add this CTP to the proactive_CS list
 *  5. At the end create AdaptationRequest to the task in proactive_task_migration and proactive_CS list
 */
void prediction_analysis(Application * app){

	CS_path * ctp;
	Task * task, * producer_task;
	int proactive_TM[app->tasks_number];
	unsigned int proactive_CS[ MAX_TASKS_APP*(SUBNETS_NUMBER-1)];
	int master_address, number_comp_task, add_to_list;
	int proactive_TM_size;
	int cpu_util;

	proactive_TM_size = 0;

	puts("\nStarting prediction...\n");

	//Implementation of step 1 - Proactive task migration
	for(int i=0; i<app->tasks_number; i++){

		task = &app->tasks[i];

		//Proactive check at computation level, check if the task have more than 50% of computation time
		if (task->computation_profile > 50 && !task->pending_TM){

			puts("\nAnalyzing task -> "); puts(itoa(task->id)); putsv(" computation prof = ", task->computation_profile);

			//Check if the task is mapped togheter another computational intensive task
			master_address = get_master_address(task->allocated_proc);

			if (master_address != net_address){
				number_comp_task = get_remote_CPU_computation_task_number(task->allocated_proc);
				cpu_util = get_remote_CPU_utilization(task->allocated_proc);
			} else {
				number_comp_task = get_number_computation_task(task->allocated_proc);
				cpu_util = get_proc_utilization(task->allocated_proc);
			}

			putsv("Number of computational intensive task at same proc = ", number_comp_task);

			//This IF test if there is another computational task togheter task
			if (number_comp_task > 1 || (cpu_util > task->utilization && cpu_util > 75)){

				add_to_list = 1;
				//If yes, add to the list, but before verifies if there is another task added that is at the same proc
				for (int j=0; j<proactive_TM_size; j++){

					//Implementation of step 2
					if(proactive_TM[j] == task->allocated_proc){

						add_to_list = 0; //Signals to not add in the list

						break;
					}
				} //end for

				if (add_to_list){
					proactive_TM[proactive_TM_size++] = task->id; //Add and increment the size

					add_adaptation_request(task, 0, TaskMigration);

					puts("Task added in proactive_TM list\n");
				}

			}//end if
		}//end if
	}//end for

	//Implementation of step 3 - Proactive CS
	for(int i=0; i<app->tasks_number; i++){

		task = &app->tasks[i];

		if (!task->pending_TM && task->communication_profile && (task->communication_profile > 15 || task->communication_profile >= task->computation_profile) ){

			puts("\nAnalyzing task -> "); puts(itoa(task->id)); putsv(" communication prof = ", task->communication_profile);

			add_to_list = 1;

			for (int k=0; k<proactive_TM_size; k++){

				if (proactive_TM[k] == task->id){
					add_to_list = 0;
					break;
				}
			}

			if (!add_to_list){
				//puts("Skkiped - task already in TM list\n");
				continue;
			}

			//Now, look for all CPT where task is the consumer
			for(int j=0; j<app->tasks_number; j++){

				producer_task = &app->tasks[j];

				if(producer_task == task || producer_task->pending_TM)
					continue;

				//Test if the producer is in proactive_TM
				for (int k=0; k<proactive_TM_size; k++){

					if(proactive_TM[k] == producer_task->id){

						add_to_list = 0;
						break;
					}//end if
				}//end for proactive_TM

				if (!add_to_list)
					continue;

				//Now test if the task is a consumer from the producer
				for (int c = 0; c < producer_task->consumer_tasks_number; c++) {

					ctp = &producer_task->consumer_tasks[c];

					//This if test if the task is a consumer in a PS subnet
					if (ctp->CS_path == -1 && ctp->id == task->id){

						puts("CTP added: "); puts(itoa(producer_task->id)); putsv(" -> ", task->id);

						add_adaptation_request(producer_task, ctp, CircuitSwitching);
						//Add as proactive CS
						//proactive_CS[proactive_CS_size++] = producer_task->id << 16 | task->id;

						break; //Stop the search
					}//end if
				}//end for  producer_task->consumer_tasks_number
			} // end for app->tasks_number for producer
		}//end if
	}//end for task

	puts("\nEnd prediction --------\n");
}

