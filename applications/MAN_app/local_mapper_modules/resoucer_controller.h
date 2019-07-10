/*
 * resoucer_controller.h
 *
 *  Created on: Jul 4, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_RESOUCER_CONTROLLER_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_RESOUCER_CONTROLLER_H_

#include "../../Management_Application/local_mapper_modules/application.h"
#include "../../Management_Application/local_mapper_modules/globals.h"
#include "../../Management_Application/local_mapper_modules/processors.h"


/** This function is called by kernel manager inside it own code and in the modules: reclustering and cluster_scheduler.
 * It is called even when a task is mapped into a processor, by normal task mapping or reclustering.
 * Automatically, this function update the Processors structure by calling the add_task function
 * \param cluster_id Index of cluster to allocate the page
 * \param proc_address Address of the processor that is receiving the task
 * \param task_ID ID of the allocated task
 */
void page_used(int proc_address, int task_ID){

	//puts("Page used proc: "); puts(itoh(proc_address)); putsv(" task id ", task_ID);
	add_task(proc_address, task_ID);

	free_resources--;
}

/** This function is called by manager inside it own code and in the modules: reclustering and cluster_scheduler.
 * It is called even when a task is removed from a processor.
 * Automatically, this function update the Processors structure by calling the remove_task function
 * \param cluster_id Index of cluster to remove the page
 * \param proc_address Address of the processor that is removing the task
 * \param task_ID ID of the removed task
 */
void page_released(int proc_address, int task_ID){

	//puts("Page released proc: "); puts(itoh(proc_address)); putsv(" task id ", task_ID);
	remove_task(proc_address, task_ID);

	free_resources++;
}

/**This heuristic maps all task of an application
 * Note that some task can not be mapped due the cluster is full or the processors not satisfies the
 * task requiriments. In this case, the reclustering will be used after the application_mapping funcion calling
 * into the kernel_master.c source file
 * Clearly the tasks that need reclustering are those one that have he allocated_processor equal to -1
 * \param cluster_id ID of the cluster where the application will be mapped
 * \param app_id ID of the application to be mapped
 * \return 1 if mapping OK, 0 if there are no available resources
*/


int application_mapping(int app_id){

	Application * app;
	Task *t;
	int proc_address;

	app = get_application_ptr(app_id);

	//Puts("\napplication_mapping\n");

	for(int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		//Search for static mapping
		if (t->allocated_proc != -1){
			proc_address = t->allocated_proc;
			Puts("Task id "); Puts(itoa(t->id)); Puts(" statically mapped at processor "); Puts(itoh(proc_address)); Puts("\n");
		} else
			//Calls task mapping algorithm
			proc_address = map_task(t->id);


		if (proc_address == -1){

			return 0;

		} else {

			t->allocated_proc = proc_address;

			page_used(proc_address, t->id);

		}
	}

	Puts("App have all its task mapped\n");

	return 1;
}


/** Maps a task into a cluster processor. This function only selects the processor not modifying any management structure
 * The mapping heuristic is based on the processor's utilization (slack time) and the number of free_pages
 * \param task_id ID of the task to be mapped
 * \return Address of the selected processor
 */
int map_task(int task_id){

	int proc_address;
	int canditate_proc = -1;
	int max_slack_time = -1;
	int slack_time;

	//Else, map the task following a CPU utilization based algorithm
	for(int i=0; i<MAX_PROCESSORS; i++){

		proc_address = get_proc_address(i);

		if (get_proc_free_pages(proc_address) > 0){

			slack_time = get_proc_slack_time(proc_address);

			if (max_slack_time < slack_time){
				canditate_proc = proc_address;
				max_slack_time = slack_time;
			}
		}
	}

	if (canditate_proc != -1){

		Puts("Task mapping for task "); Puts(itoa(task_id)); Puts(" maped at proc "); Puts(itoh(canditate_proc)); Puts("\n");

		return canditate_proc;
	}

	Puts("WARNING: no resources available in cluster to map task "); Puts(itoa(task_id)); Puts("\n");
	return -1;
}


#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_RESOUCER_CONTROLLER_H_ */
