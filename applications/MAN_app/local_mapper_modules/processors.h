/*
 * processors.h
 *
 *  Created on: Jul 4, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_PROCESSORS_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_PROCESSORS_H_

#include "globals.h"
#include "resoucer_controller.h"
#include "../common_include.h"

/**
 * \brief This structure store variables used to manage the processors attributed by the kernel master
 */
typedef struct {
	int address;						//!<Processor address in XY
	int free_pages;						//!<Number of free memory pages
	int slack_time;						//!<Slack time (idle time), represented in percentage
	unsigned int total_slack_samples;	//!<Number of slack time samples
	int task[MAX_LOCAL_TASKS]; 			//!<Array with the ID of all task allocated in a given processor
} Processor;


Processor processors[MAX_PROCESSORS];	//!<Processor array


void init_procesors();

void update_proc_slack_time(int, int);

int get_proc_slack_time(int);

void add_procesor(int);

void add_task(int, int);

void remove_task(int, int);

int get_proc_free_pages(int);

int get_proc_address(int);

int get_task_location(int);



/**Internal function to search by a processor - not visible to the other software part
 * \param proc_address Processor address to be searched
 * \return The processor pointer
 */
Processor * search_processor(int proc_address){

	for(int i=0; i<MAX_PROCESSORS; i++){
		if (processors[i].address == proc_address){
			return &processors[i];
		}
	}

	Puts("ERROR: Processor not found "); Puts(itoa( proc_address )); Puts("\n");
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

	Puts("Adding processor address "); Puts(itoh(proc_address)); Puts("\n");

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
		Puts("ERROR: Free pages is 0 - not add task enabled for proc "); Puts(itoa( proc_address )); Puts("\n");
		while(1);
	}

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if (p->task[i] == -1){
			add_i = i;
		}
		if (p->task[i] == task_id){
			Puts("ERROR: Task already added "); Puts(itoa( task_id )); Puts("\n");
			while(1);
		}
	}

	//add_i will be -1 when there is not more slots free into proc
	if (add_i > -1){
		p->task[add_i] = task_id;
		p->free_pages--;
	} else {
		Puts("ERROR: Not free slot into processor "); Puts(itoa( proc_address )); Puts("\n");
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
		Puts("ERROR: All pages free for proc "); Puts(itoa( proc_address )); Puts("\n");
		while(1);
	}

	for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if (p->task[i] == task_id){
			p->task[i] = -1;
			p->free_pages++;
			return;
		}
	}

	Puts("ERROR: Task not found to remove "); Puts(itoa( task_id )); Puts("\n");
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
int get_task_location(int task_id){

	for(int i=0; i<MAX_PROCESSORS; i++){

		if (processors[i].free_pages == MAX_LOCAL_TASKS || processors[i].address == -1){
			continue;
		}

		for(int t=0; t<MAX_LOCAL_TASKS; t++){

			if (processors[i].task[t] == task_id){

				return processors[i].address;
			}
		}
	}

	Puts("Warining: Task not found at any processor "); Puts(itoa( task_id )); Puts("\n");
	return -1;
}

/**Initializes the processors's array with invalid values
 */
void init_procesors(){

	unsigned int x_final, y_final;

	x_final = XCLUSTER + cluster_x_offset;
	y_final = YCLUSTER + cluster_y_offset;

	for(int i=0; i<MAX_PROCESSORS; i++){
		processors[i].address = -1;
		processors[i].free_pages = MAX_LOCAL_TASKS;
		processors[i].slack_time = 100;
		for(int t=0; t<MAX_LOCAL_TASKS; t++){
			processors[i].task[t] = -1;
		}
	}

	for(int x=cluster_x_offset; x < x_final; x++){
		for(int y=cluster_y_offset; y < y_final; y++){
			add_procesor( ( x << 8 | y ) );
		}
	}

}


#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_PROCESSORS_H_ */
