/*!\file processors.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * This module implements function relative to the slave processor management by the manager kernel.
 * This modules is used only by the manager kernel
 */

#include "processors.h"
#include "applications.h"
#include "utils.h"

Processor processors[MAX_CLUSTER_SLAVES];	//!<Processor array

/**Internal function to search by a processor - not visible to the other software part
 * \param proc_address Processor address to be searched
 * \return The processor pointer
 */
Processor * search_processor(int proc_address){

	for(int i=0; i<MAX_CLUSTER_SLAVES; i++){
		if (processors[i].address == proc_address){
			return &processors[i];
		}
	}

	putsv("ERROR: Processor not found ", proc_address);
	while(1);
	return 0;
}

/**Gets the processor address stored in the index parameter
 * \param index Index of the processor address
 * \return The processor address (XY)
 */
int get_proc_address(int index){

	return processors[index].address;
}

/**Updates the processor slack time
 * \param proc_address Processor address
 * \param slack_time Slack time in percentage
 */
void update_proc_slack_time(int proc_address, int slack_time){

	Processor * p = search_processor(proc_address);

	p->slack_time += slack_time;

	p->total_slack_samples++;

}

/**Gets the processor slack time
 * \param proc_address Processor address
 * \return The processor slack time in percentage
 */
int get_proc_slack_time(int proc_address){

	Processor * p = search_processor(proc_address);

	if (p->total_slack_samples == 0)
		return 100;

	return (p->slack_time/p->total_slack_samples);
}


/** Gets the next found task ID of the task running on proc_address
 * \param proc_address Target processor address
 * \return BE task id, -1 if not found
 */
int get_BE_task_id(int proc_address){

	Processor * p = search_processor(proc_address);

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if (p->task[i] != -1 && p->utilization[i] == 0)
			return p->task[i];
	}

	return -1;
}


/** Get the total of computation profile sum of this PE
 */
int get_number_computation_task(int proc_address){

	Processor * p;
	Application * app;
	Task * task;
	int current_task_id;
	int number_comp_task = 0;

	p = search_processor(proc_address);

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		//If there is another RT running different from the task
		if (p->task[i] != -1 && p->utilization[i] !=0){

			current_task_id = p->task[i];

			app = search_application(current_task_id >> 8);

			if (app){

				task = get_task_ptr(app, current_task_id);

				if (task->computation_profile >= 25){
					puts("\nThere are other computation intesive task\n");
					number_comp_task++;
				}
			}
		}
	}

	return number_comp_task;
}

/** Gets the first PE that is at least one free page and no RT tasks
 * \return The processor XY address
 */
int gets_PE_without_RT_task(){
	int candidate = -1, rt_task_running;
	Processor * p;

	for(int i=0; i<MAX_CLUSTER_SLAVES; i++){
		rt_task_running = 0;
		p = &processors[i];

		//Test if the processor has RT tasks
		for(int i=0; i<MAX_LOCAL_TASKS; i++){
			if (p->task[i] != -1 && p->utilization[i] != 0){
				rt_task_running = 1;
				break;
			}
		}

		//If the PE not have any RT task and it has free pages then
		if (!rt_task_running && processors[i].free_pages > 0){

			//Gives priority to the processor with the higher number of BE tasks
			//This group BE task in a single PE
			if (candidate == -1)
				candidate = i;
			else if (processors[candidate].free_pages > p->free_pages)
				candidate = i;

		}
	}//end for

	if (candidate != -1)
		return processors[candidate].address;

	return -1;
}


/** Gets the number of RT tasks allocated on the processor
 * \param proc_address Target processor address
 * \return The number of RT tasks running on PE
 */
int get_RT_task_number(int proc_address){

	int rt_tasks;
	Processor * p = search_processor(proc_address);

	rt_tasks = 0;

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if (p->task[i] != -1 && p->utilization[i] > 0)
			rt_tasks++;
	}

	return rt_tasks;
}


/**Updates the processor utilization
 * \param proc_address Processor address
 * \param task_id Task ID of the
 * \param task_utilization Utilization of the task in percentage
 */
void update_proc_utilization(int proc_address, int task_id, int task_utilization){
	Processor * p = search_processor(proc_address);

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if (p->task[i] == task_id){
			p->utilization[i] = task_utilization;
		}
	}
}

/** Gets the task utilization
 * \param task_id Task ID of the
 * \param proc_address Processor address
 * \return The task utilization in percentage
 */
int get_task_utilization(int task_id, int proc_address){

	Processor * p = search_processor(proc_address);

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if (p->task[i] == task_id)
			return p->utilization[i];
	}

	putsv("ERROR: task not found ", task_id);
	while(1);

	return 0;
}

/**Gets the processor utilization
 * \param proc_address Processor address
 * \return The processor utilization in percentage
 */
int get_proc_utilization(int proc_address){

	Processor * p = search_processor(proc_address);

	int proc_utilization = 0;

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		proc_utilization += p->utilization[i];
	}

	return proc_utilization;

}

/**Add a valid processor to the processors' array
 * \param proc_address Processor address to be added
 */
void add_procesor(int proc_address){

	Processor * p = search_processor(-1); //Searches for a free slot

	p->free_pages = MAX_LOCAL_TASKS;

	p->address = proc_address;

	p->slack_time = 0;

	p->total_slack_samples = 0;

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		p->task[i] = -1;
	}

}

/**Add a task into a processor. Called evenly when a task is mapped into a processor.
 * \param proc_address Processor address
 * \param task_id Task ID to be added
 */
void add_task(int proc_address, int task_id){

	Processor * p = search_processor(proc_address);

	//Index to add the task
	int add_i = -1;

	if (p->free_pages == 0){
		putsv("ERROR: Free pages is 0 - not add task enabled for proc", proc_address);
		while(1);
	}

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if (p->task[i] == -1){
			add_i = i;
		}
		if (p->task[i] == task_id){
			putsv("ERROR: Task already added ", task_id);
			while(1);
		}
	}

	//add_i will be -1 when there is not more slots free into proc
	if (add_i > -1){
		p->task[add_i] = task_id;
		p->utilization[add_i] = 0;
		p->free_pages--;
	} else {
		putsv("ERROR: Not free slot into processor ", proc_address);
		while(1);
	}
}

/**Remove a task from a processor. Called evenly when a task is unmapped (removed) from a processor.
 * \param proc_address Processor address
 * \param task_id Task ID to be removed
 */
void remove_task(int proc_address, int task_id){

	Processor * p = search_processor(proc_address);

	if (p->free_pages == MAX_LOCAL_TASKS){
		putsv("ERROR: All pages free for proc", proc_address);
		while(1);
	}

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if (p->task[i] == task_id){
			p->task[i] = -1;
			p->utilization[i] = 0;
 			p->free_pages++;
			return;
		}
	}

	puts("ERROR: Task not found to remove "); puts(itoa(task_id)); puts(" proc "); puts(itoh(proc_address)); puts("\n");
	while(1);

}

/**Gets the total of free pages of a given processor
 * \param proc_address Processor address
 * \return The number of free pages
 */
int get_proc_free_pages(int proc_address){

	Processor * p = search_processor(proc_address);

	return p->free_pages;

}

/**Searches for a task location by walking for all processors within processors' array
 * \param task_id Task ID
 * \return The task location, i.e., the processor address that the task is allocated
 */
/*int get_task_location(int task_id){

	for(int i=0; i<MAX_CLUSTER_SLAVES; i++){

		if (processors[i].free_pages == MAX_LOCAL_TASKS || processors[i].address == -1){
			continue;
		}

		for(int t=0; t<MAX_LOCAL_TASKS; t++){

			if (processors[i].task[t] == task_id){

				return processors[i].address;
			}
		}
	}

	putsv("Warining: Task not found at any processor ", task_id);
	return -1;
}*/

/**Initializes the processors's array with invalid values
 */
void init_procesors(){
	for(int i=0; i<MAX_CLUSTER_SLAVES; i++){
		processors[i].address = -1;
		processors[i].free_pages = MAX_LOCAL_TASKS;
		processors[i].slack_time = 100;
		for(int t=0; t<MAX_LOCAL_TASKS; t++){
			processors[i].task[t] = -1;
			processors[i].utilization[t] = 0;
		}
	}
}
