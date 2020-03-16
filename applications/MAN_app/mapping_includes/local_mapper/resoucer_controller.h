/*
 * resoucer_controller.h
 *
 *  Created on: Jul 4, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_RESOUCER_CONTROLLER_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_RESOUCER_CONTROLLER_H_

#include "application.h"
#include "processors.h"
#include "globals.h"


int diamond_search_initial(int);

int reclustering_map(int);

/** This function is called by kernel manager inside it own code and in the modules: reclustering and cluster_scheduler.
 * It is called even when a task is mapped into a processor, by normal task mapping or reclustering.
 * Automatically, this function update the Processors structure by calling the add_task function
 * \param cluster_id Index of cluster to allocate the page
 * \param proc_address Address of the processor that is receiving the task
 * \param task_ID ID of the allocated task
 */
void page_used(int proc_address, int task_ID){

	//Puts("Page used proc: "); Puts(itoh(proc_address)); putsv(" task id ", task_ID);
	add_task(proc_address, task_ID);

	free_resources--;

	//Puts("Used: "); Puts(itoh(proc_address)); putsv(" free ", free_resources);
}

/** This function is called by manager inside it own code and in the modules: reclustering and cluster_scheduler.
 * It is called even when a task is removed from a processor.
 * Automatically, this function update the Processors structure by calling the remove_task function
 * \param cluster_id Index of cluster to remove the page
 * \param proc_address Address of the processor that is removing the task
 * \param task_ID ID of the removed task
 */
void page_released(int proc_address, int task_ID){

	//Puts("Page released proc: "); Puts(itoh(proc_address)); putsv(" task id ", task_ID);
	remove_task(proc_address, task_ID);

	free_resources++;

	//Puts("Released: "); Puts(itoh(proc_address)); putsv(" free ", free_resources);
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
	int initial_pe_list[MAX_CLUSTER_TASKS];
	int initial_size;
	int initial_app_pe;
	int proc_addr;
	int max_avg_manhatam;
	int man_sum, man_count, man_curr;
	int xi, yi, xj, yj;

	app = get_application_ptr(app_id);

	//Puts("\napplication_mapping\n");

	max_avg_manhatam = -1;
	initial_app_pe = -1;

	//Puts("\n----------------Defining list of initial PE------------\n");
	get_initial_pe_list(initial_pe_list, &initial_size);

	if (initial_size){

		//For each free PE of the cluster
		for(int i=0; i<MAX_PROCESSORS; i++){

			proc_addr = get_proc_address(i);

			if(get_proc_free_pages(proc_addr) > 0){

				man_sum = 0;
				man_count = 0;

				xi = proc_addr >> 8;
				yi = proc_addr & 0xFF;
				//For each initial_PE compute  the avg manhatam from the proc_addr
				for(int j=0; j<initial_size; j++){

					xj = initial_pe_list[j] >> 8;
					yj = initial_pe_list[j] & 0xFF;

					//Computes the manhatam distance
					man_curr = (abs(xi - xj) + abs(yi - yj));

					man_sum = man_sum + man_curr;
					man_count++;
				}

				//After the for computes the mean
				man_curr = (man_sum / man_count);
				//Puts("Current Avg mean for PE "); Puts(itoh(proc_addr)); putsv(" is ", man_curr);

				if (man_curr > max_avg_manhatam){
					max_avg_manhatam = man_curr;
					initial_app_pe = proc_addr;
					//Puts("PE selected\n");
				}
			}

		}

	} else {
		initial_app_pe = (cluster_x_offset << 8 | (cluster_y_offset + MAPPING_YCLUSTER -1));
		//Puts("Very first time\n");
	}

	Puts("SELECTED initial addr: "); Puts(itoh(initial_app_pe)); Puts("\n");



	for(int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		//Search for static mapping
		if (t->allocated_proc != -1){
			proc_address = t->allocated_proc;
			Puts("Task id "); Puts(itoa(t->id)); Puts(" statically mapped at processor "); Puts(itoh(proc_address)); Puts("\n");
		} else
			//Calls task mapping algorithm
			proc_address = diamond_search_initial(initial_app_pe);

		if (proc_address == -1){

			return 0;

		} else {

			t->allocated_proc = proc_address;

			page_used(proc_address, t->id);

			Puts("Task id "); Puts(itoa(t->id)); Puts(" dynamically mapped at processor "); Puts(itoh(proc_address)); Puts("\n");

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
int reclustering_map(int ref_proc){

	int ref_x, ref_y, curr_x, curr_y, proc_address;
	int curr_man, min_man, sel_proc;

	ref_x = ref_proc >> 8;
	ref_y = ref_proc & 0xFF;

	min_man = (XDIMENSION*YDIMENSION);
	sel_proc = -1;

	//Else, selects the pe with the minimal manhatam to the initial proc
	for(int i=0; i<MAX_PROCESSORS; i++){

		proc_address = get_proc_address(i);

		curr_x = proc_address >> 8;
		curr_y = proc_address & 0xFF;

		if (get_proc_free_pages(proc_address) > 0){

			curr_man = (abs(ref_x - curr_x) + abs(ref_y - curr_y));

			if (curr_man < min_man){
				min_man = curr_man;
				sel_proc = proc_address;
			}
		}
	}

	return sel_proc;
}

/** Searches following the diamond search paradigm. The search occurs only inside cluster
 * \param current_allocated_pe Current PE of the target task
 * \param last_proc The last proc returned by diamond_search function. 0 if is the first call
 * \return The selected processor address. -1 if not found
 */
int diamond_search_initial(int begining_core){

	int ref_x, ref_y, max_round, hop_count, max_tested_proc;//XY address of the current processor of the task
	int proc_x, proc_y, proc_count;
	int candidate_proc;
	int max_x, max_y, min_x, min_y;

	ref_x = (begining_core >> 8);
	ref_y = (begining_core & 0xFF);

	//Return the own beggining core if it is free
	if(get_proc_free_pages(begining_core)){
		//Puts("Mapped at beggining core\n");
		return begining_core;
	}

	//Puts("xi: "); Puts(itoh(cluster_x_offset));

	max_x = (MAPPING_XCLUSTER+cluster_x_offset);
	max_y = (MAPPING_YCLUSTER+cluster_y_offset);
	min_x = (int) cluster_x_offset;
	min_y = (int) cluster_y_offset;

	max_round = 0;
	hop_count = 0;
	proc_count = 1;

	max_tested_proc = MAX_PROCESSORS;

	//Search candidate algorithm
	while(proc_count < max_tested_proc){

		//max_round count the number of processor of each round
		max_round+=4;
		hop_count++; //hop_count is used to position the proc_y at the top of the task's processor

		proc_x = ref_x;
		proc_y = ref_y + hop_count;

		//Puts(itoa(proc_x)); putsv("x", proc_y);

		candidate_proc = -1;//Init the variable to start the looping below
		//putsv("-------- New round: ", max_round);

		//Walks for all processor of the round
		for(int r=0; r<max_round; r++){

			//Puts("Testando addr: "); Puts(itoa(proc_x)); Puts("x"); Puts(itoa(proc_y)); Puts("\n");

			//Test if the current processor is out of the system dimension
			if (min_x <= proc_x && max_x > proc_x && min_y <= proc_y && max_y > proc_y){ // Search all PEs

				//Puts("Entrou addr: "); Puts(itoa(proc_x)); Puts("x"); Puts(itoa(proc_y)); Puts("\n");
				//Increment the number of valid processors visited
				proc_count++;

				//Tests if the processor is available, if yes, mark it as used in free_core_map and return
				if(get_proc_free_pages(proc_x << 8 | proc_y)){
				//if (free_core_map[proc_x][proc_y] == 1){
					candidate_proc = proc_x << 8 | proc_y;
					//Puts("Proc selected: "); Puts(itoh(candidate_proc)); Puts("\n");
					return candidate_proc;
				}

			}

			//These if walk over the processor addresses
			if (proc_y > ref_y && proc_x <= ref_x){ //up to left
					proc_y--;
					proc_x--;
					//Puts("up left\n");
			} else if (proc_y <= ref_y && proc_x < ref_x){//left to bottom
					proc_y--;
					proc_x++;
					//Puts("left to bottom\n");
			} else if (proc_x >= ref_x && proc_y < ref_y){//bottom to right
					proc_y++;
					proc_x++;
					//Puts("bottom to right\n");
			} else {//right to up
					proc_y++;
					proc_x--;
					//Puts("right to up\n");
			}
		}

	}

	return -1;
}


/**Request the SDN utilization of a list of SDN routers that are free to receive one task at least
 *
 */
void request_SDN_utilization(){

}

void handle_SDN_utilization_reply(){

}



#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_RESOUCER_CONTROLLER_H_ */
