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
#include "circuit_switching.h"
#include "globals.h"

#define CS_NETS 		(SUBNETS_NUMBER-1)
#define PORT_NUMBER		6 //Total number of ports (LOCAL_IN, LOCAL_OUT, WEAST, EAST, NORTH, SOUTH)


int diamond_search_initial(int, int);

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


#if 1

int select_initial_PE(int * initial_pe_list, int initial_size, char * valid_pe_list, int secure_app){
	int max_avg_manhatam, initial_app_pe, intial_pe_index;
	int man_sum, man_count, man_curr;
	int xi, yi, xj, yj;
	int proc_addr;

	Puts("\nselect_initial_PE\n");

	max_avg_manhatam = -1;
	initial_app_pe = -1;
	intial_pe_index = -1;

	if (initial_size){

		//For each free PE of the cluster
		for(int i=0; i<MAX_PROCESSORS; i++){

			putsv("Testing index = ", i);

			//Tests if the selected PE is a excluded PE
			if (valid_pe_list[i] == 0){
				Puts("Intial excluded\n");
				Puts("...\n");
				continue;
			}

			proc_addr = get_proc_address(i);

			Puts("Candidato address: "); Puts(itoh(proc_addr)); Puts("\n");

			//So considera um PE que esta vazio em caso de app. segura, ou qualquer PE que possui um espaco vazio, caso de app nao segura
			if( (secure_app && get_proc_free_pages(proc_addr) == MAX_LOCAL_TASKS) || ( !secure_app && get_proc_free_pages(proc_addr) > 0) ){

				Puts("Proc entrou\n");

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
					intial_pe_index = i;
					Puts("PE selected\n");
				}
			}

		}

	} else {
		//The first initial PE is manually sected at the top left corner
		initial_app_pe = (cluster_x_offset + MAPPING_XCLUSTER - 1) << 8 | (cluster_y_offset + MAPPING_YCLUSTER -1);
		Puts("Very first time\n");
	}

	if (intial_pe_index != -1){
		valid_pe_list[intial_pe_index] = 1;
		putsv("Excluded PE index: ", intial_pe_index);
	}

	Puts("SELECTED initial addr: "); Puts(itoh(initial_app_pe)); Puts("\n");

	return initial_app_pe;
}

/*Heuristica de mapeamento aging e CS
 * OBS: Tarefas de diferentes apps nao podem compartilhar mesmo PE
 * 1. Selecionar o initial PE
 * 2. Executar o mapeamento diamante da applicacai neste initial PE e anotar a utilizacao CS acumulada daquela regiao.
 * 3. Deslocar o initial PE para o proximo PE livre, exceto o ultimo selecionado.
 * 4. Repetir esse processo até:
		- N vezes (N numbero de PEs do cluster) ou um valor parametrizavel
		- utilizacao de CS esteja abaixo de um limiar
 * */

int application_mapping(int app_id){
	Application * app;
	Task *t;
	char mapping_completed;
	int N;
	int proc_address;
	int initial_pe_list[MAX_CLUSTER_TASKS];
	int initial_size;
	int initial_app_pe;
	int x_proc, y_proc;
	int bb_min_x, bb_min_y, bb_max_x, bb_max_y;
	int current_bb_utilization, ref_bb_utilization, bb_utilization_TH, ref_initial_address;

	//Used to store the position of the valid_initial_PE
	char valid_initial_PE[MAX_PROCESSORS];

	/*################## STEP 0 - INITIALIZATION AND SEARCH FOR THE FIRST INITIAL PE ####################### */

	//Set the list of valid initial PEs
	for(int i=0; i<MAX_PROCESSORS; i++){
		valid_initial_PE[i] = 1;
	}

	//Set the reference utilization to the worst value
	ref_bb_utilization = 0;
	ref_initial_address = -1;

	//Set mapping completed as false
	mapping_completed = 0;

	//Get the pplication ptr
	app = get_application_ptr(app_id);

	//Puts("\n----------------Defining list of initial PE------------\n");
	get_initial_pe_list(initial_pe_list, &initial_size);

	Puts("\n\nStarting APP MAPPING. ID: "); Puts(itoa(app->app_ID)); putsv(" task number: ", app->tasks_number);

	//Algorithm that selects the initial PE
	initial_app_pe = select_initial_PE(initial_pe_list, initial_size, valid_initial_PE, app->is_secure);

	//Case there is not possible to find a initial PE then return zero to signals that the system is full
	if (initial_app_pe == -1){
		Puts("Return 0 - None initial PE found\n");
		return 0;
	}


	if (app->is_secure){

		Puts("\nApp is secure, starting heuristics...\n");

		//Computes the utilization threshold based on the bounding box
		bb_utilization_TH = PORT_NUMBER * CS_NETS;

		N = MAX_PROCESSORS;

		while(N > 0){

			/*################## STEP 1 - DIAMON-BASED MAPPING OF TASKS USING THE INITIAL PE ####################### */

			//Initializes the bounding box limits to the worst-cases
			bb_min_x = MAPPING_XCLUSTER;
			bb_min_y = MAPPING_YCLUSTER;
			bb_max_x = 0;
			bb_max_y = 0;

			//Executa o mapeamento em diamante ao redor do PE inicial
			for(int i=0; i<app->tasks_number; i++){

				t = &app->tasks[i];

				//Search for static mapping
				if (t->allocated_proc != -1){
					proc_address = t->allocated_proc;
				} else {
					proc_address = diamond_search_initial(initial_app_pe, app->app_ID);
				}


				//Case the diamind search cannot find any free PE to the secure task, the mapping return zero
				if (proc_address == -1){
					Puts("Returning zero, not resource available\n");
					return 0;

				} else {

					Puts("Task "); Puts(itoa(t->id)); Puts(" mapped to "); Puts(itoh(proc_address)); Puts("\n");
					t->allocated_proc = proc_address;
					page_used(proc_address, t->id);

					//Update bounding box limits
					x_proc = proc_address >> 8;
					y_proc = proc_address & 0xFF;

					//Update x axis
					if (x_proc < bb_min_x)
						bb_min_x = x_proc;
					if (x_proc > bb_max_x)
						bb_max_x = x_proc;

					//Update y axis
					if (y_proc < bb_min_y)
						bb_min_y = y_proc;
					if (y_proc > bb_max_y)
						bb_max_y = y_proc;
				}
			}

			/*################## STEP 2 - CS UTILIZATION SUM INTO THE BOUNDING BOX ####################### */
			//Remove the offset
			bb_min_x = bb_min_x - cluster_x_offset;
			bb_max_x = bb_max_x - cluster_x_offset;
			bb_min_y = bb_min_y - cluster_y_offset;
			bb_max_y = bb_max_y - cluster_y_offset;

			//Reuses x_proc to store the number of PE of the bounding box
			x_proc = (bb_max_x-bb_min_x+1)*(bb_max_y-bb_min_y+1);

			Puts("Final BB: "); Puts(itoh(bb_min_x << 8 | bb_min_y)); Puts(" - ");
			Puts(itoh(bb_max_x << 8 | bb_max_y)); Puts("\n");

			current_bb_utilization = compute_bounding_box_cs_utilization(bb_min_x, bb_min_y, bb_max_x, bb_max_y);


			/*################## STEP 3 - TEST IF THE UTILIZATION FILLS THE THRESHOLD OR IS BETTER THAN REFERENCE ####################### */

			putsv("\nBB util TH: ", bb_utilization_TH);
			putsv("Computed BB util: ", current_bb_utilization);
			putsv("Ref BB util: ", ref_bb_utilization);

			//From this moment the total utilization is converted to a utilization rate, dividing the accumulated utilization
			//by the number of PE of the bounding box
			current_bb_utilization = current_bb_utilization / x_proc;

			putsv("Computed BB ratio: ", current_bb_utilization);

			//If YES then the current utilization is good enough to stop the mapping
			if (current_bb_utilization >= bb_utilization_TH){
				Puts("BEST util\n");
				mapping_completed = 1;
				Puts("Mapping completed, exiting while N\n");
				break; //while(N > 0)

			} else {

				Puts("Mapping not completed, releasing pages\n");
				mapping_completed = 0;
				//Reverts all page used
				for(int i=0; i<app->tasks_number; i++){
					//Reusing x_proc
					t = &app->tasks[i];
					page_released(t->allocated_proc, t->id);
				}

			}

			//Test if the current bb utilization is best than the reference
			if (current_bb_utilization > ref_bb_utilization){
				ref_bb_utilization = current_bb_utilization;
				ref_initial_address = initial_app_pe;
				Puts("Better util\n");

			} else {
				//Case YES, repeats the algorithm that selects the initial PE
				proc_address = select_initial_PE(initial_pe_list, initial_size, valid_initial_PE, app->is_secure);

				//Case there is no more initial proc available, then get the one that has the best cs utilization
				if (proc_address == -1){
					initial_app_pe = ref_initial_address;
					break; //while(N > 0)
				}
			}

			N--;
		}//End while(N > 0)

	}//End if (app->is_secure)


	/*################## FINAL STEP - MAPPS INDEED THE TASKS INTO THE SYSTEM ####################### */
	if (!mapping_completed){
		//Executa o mapeamento baseado no Initial core
		for(int i=0; i<app->tasks_number; i++){

			t = &app->tasks[i];

			//Search for static mapping
			if (t->allocated_proc != -1){
				proc_address = t->allocated_proc;
				Puts("Task id "); Puts(itoa(t->id)); Puts(" statically mapped at processor "); Puts(itoh(proc_address)); Puts("\n");
			} else
				//Diamon retorna com o offset
				if(app->is_secure)
					proc_address = diamond_search_initial(initial_app_pe, app->app_ID);
				else
					proc_address = diamond_search_initial(initial_app_pe, 0);

			if (proc_address == -1){

				return 0;

			} else {

				t->allocated_proc = proc_address;

				page_used(proc_address, t->id);

				Puts("Task id "); Puts(itoa(t->id)); Puts(" dynamically mapped at processor "); Puts(itoh(proc_address)); Puts("\n");

			}
		}
	}


	Puts("\nApp have all its task mapped\n\n");

	return 1;
}
#endif

/*Original Heuristic*/
#if 0
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
#endif


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
int diamond_search_initial(int begining_core, int secure_app_id){

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
				if ( (secure_app_id && check_security_allocation_requirements((proc_x << 8 | proc_y), secure_app_id)) ||
				   (!secure_app_id && get_proc_free_pages(proc_x << 8 | proc_y)) ){

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
