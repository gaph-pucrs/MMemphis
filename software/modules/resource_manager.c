/*!\file resource_manager.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief Selects where to execute a task and application
 * \detailed Resource manager implements the cluster resources management,
 * task mapping, application mapping, and also can implement task migration heuristics.
 * Adittionally it have a function named: SearchCluster, which selects the cluster to send an application. This
 * function in only used in the global master mode
 */

#include "resource_manager.h"

#include "../../include/kernel_pkg.h"
#include "utils.h"
#include "processors.h"
#include "applications.h"
#include "reclustering.h"

unsigned int clusterID;					//!<Current cluster ID - unique global variable externed to other modules

/** Set the current cluster index
 *\param cluster_id Index of this cluster
 */
void set_cluster_ID(unsigned int cluster_id){
	clusterID = cluster_id;
}

/** Receives a slave address and tells which is its master address
 *  \param slave_address The XY slave address
 *  \return The master address of the slave. Return -1 if there is no master (ERROR situation)
 */
int get_master_address(int slave_address){

	ClusterInfo *cf;
	int proc_x, proc_y;

	proc_x = slave_address >> 8;
	proc_y = slave_address & 0xFF;

	for(int i=0; i<CLUSTER_NUMBER; i++){
		cf = &cluster_info[i];
		if (cf->xi <= proc_x && cf->xf >= proc_x && cf->yi <= proc_y && cf->yf >= proc_y){
			return get_cluster_proc(i);
		}
	}

	puts("ERROR: no master address found\n");
	while(1);
	return -1;
}

/** Allocate resources to a Cluster by decrementing the number of free resources. If the number of resources
 * is higher than free_resources, then free_resourcers receives zero, and the remaining of resources are allocated
 * by reclustering
 * \param cluster_index Index of cluster to allocate the resources
 * \param nro_resources Number of resource to allocated. Normally is the number of task of an application
 */
inline void allocate_cluster_resource(int cluster_index, int nro_resources){

	//puts(" Cluster address "); puts(itoh(cluster_info[cluster_index].master_x << 8 | cluster_info[cluster_index].master_y)); puts(" resources "); puts(itoa(cluster_info[cluster_index].free_resources));

	if (cluster_info[cluster_index].free_resources > nro_resources){
		cluster_info[cluster_index].free_resources -= nro_resources;
	} else {
		cluster_info[cluster_index].free_resources = 0;
	}

	//putsv(" ALLOCATE - nro resources : ", cluster_info[cluster_index].free_resources);
}

/** Release resources of a Cluster by incrementing the number of free resources according to the nro of resources
 * by reclustering
 * \param cluster_index Index of cluster to allocate the resources
 * \param nro_resources Number of resource to release. Normally is the number of task of an application
 */
inline void release_cluster_resources(int cluster_index, int nro_resources){

	//puts(" Cluster address "); puts(itoh(cluster_info[cluster_index].master_x << 8 | cluster_info[cluster_index].master_y)); puts(" resources "); puts(itoa(cluster_info[cluster_index].free_resources));

	cluster_info[cluster_index].free_resources += nro_resources;

   // putsv(" RELEASE - nro resources : ", cluster_info[cluster_index].free_resources);
}

/** This function is called by kernel manager inside it own code and in the modules: reclustering and cluster_scheduler.
 * It is called even when a task is mapped into a processor, by normal task mapping or reclustering.
 * Automatically, this function update the Processors structure by calling the add_task function
 * \param cluster_id Index of cluster to allocate the page
 * \param proc_address Address of the processor that is receiving the task
 * \param task_ID ID of the allocated task
 */
void page_used(int cluster_id, int proc_address, int task_ID){

	//puts("Page used proc: "); puts(itoh(proc_address)); putsv(" task id ", task_ID);
	add_task(proc_address, task_ID);

	allocate_cluster_resource(cluster_id, 1);
}

/** This function is called by manager inside it own code and in the modules: reclustering and cluster_scheduler.
 * It is called even when a task is removed from a processor.
 * Automatically, this function update the Processors structure by calling the remove_task function
 * \param cluster_id Index of cluster to remove the page
 * \param proc_address Address of the processor that is removing the task
 * \param task_ID ID of the removed task
 */
void page_released(int cluster_id, int proc_address, int task_ID){

	//puts("Page released proc: "); puts(itoh(proc_address)); putsv(" task id ", task_ID);
	remove_task(proc_address, task_ID);

	release_cluster_resources(cluster_id, 1);
}

/** Maps a task into a cluster processor. This function only selects the processor not modifying any management structure
 * The mapping heuristic is based on the processor's utilization (slack time) and the number of free_pages
 * \param task_id ID of the task to be mapped
 * \param reference_processor The reference processor address used by the heuristic to map task close
 * \return Address of the selected processor
 */
int map_task(int task_id, int task_utilization){

	int canditate_proc = -1;
	int cluster_proc[MAX_CLUSTER_SLAVES];
	short int sel_i;
	int worst_value, value_aux;
	//putsv("Mapping call for task id ", task_id);

#if MAX_STATIC_TASKS
	//Test if the task is statically mapped
	for(int i=0; i<MAX_STATIC_TASKS; i++){

		//Test if task_id is statically mapped
		if (static_map[i][0] == task_id){
			puts("Task id "); puts(itoa(static_map[i][0])); puts(" statically mapped at processor"); puts(itoh(static_map[i][1])); puts("\n");

			canditate_proc = static_map[i][1];

			if (get_proc_free_pages(canditate_proc) <= 0){
				puts("ERROR: Processor not have free resources\n");
				while(1);
			}

			return canditate_proc;
		}
	}
#endif

	//IMPLEMENT THE MAPPING HEURISTC HERE
	//canditate_proc = task_mapping_heuristic(0);
	/*for (int i=0; i<MAX_CLUSTER_SLAVES;i++){
		if (get_proc_free_pages(get_proc_address(i)) > 0){
			canditate_proc = get_proc_address(i);
			break;
		}
	}*/

	//Inicialize cluster processors
	for (int i=0; i<MAX_CLUSTER_SLAVES; i++){
		if (get_proc_free_pages(get_proc_address(i)) > 0)
			cluster_proc[i] = i;
		else
			cluster_proc[i] = -1;
	}

	/*if (task_utilization > 0){

		//UTILIZATION
		worst_value = task_utilization; //maximum utilization
		sel_i = -1;
		for (int i=0; i<MAX_CLUSTER_SLAVES; i++){
			if (cluster_proc[i] == -1) continue;
			value_aux = 100 - get_proc_utilization(get_proc_address(i)); //gets the remaining utilization
			if (value_aux >= worst_value){
				worst_value = value_aux;
				if (sel_i != -1)
					cluster_proc[sel_i] = -1;
				sel_i = i;
			} else if (value_aux != 100)
				cluster_proc[i] = -1;
		}
		//print_proc(cluster_proc);
	}

	//MAX_AVG_SLACK
	worst_value = 0; //maximum utilization
	sel_i = -1;
	for (int i=0; i<MAX_CLUSTER_SLAVES; i++){
		if (cluster_proc[i] == -1) continue;
		value_aux = get_proc_slack_time(get_proc_address(i));
		if (value_aux >= worst_value){
			worst_value = value_aux;
			if (sel_i != -1)
				cluster_proc[sel_i] = -1;
			sel_i = i;
		} else if (value_aux != 100)
			cluster_proc[i] = -1;
	}

	//print_proc(cluster_proc);*/

	//MIN_ALLOCATED_TASKS
	worst_value = 0; //maximum utilization
	sel_i = -1;
	for (int i=0; i<MAX_CLUSTER_SLAVES; i++){
		if (cluster_proc[i] == -1) continue;
		value_aux = get_proc_free_pages(get_proc_address(i));
		if (value_aux >= worst_value){
			worst_value = value_aux;
			if (sel_i != -1)
				cluster_proc[sel_i] = -1;
			sel_i = i;
		} else
			cluster_proc[i] = -1;

	}

	//print_proc(cluster_proc);

	for (int i=0; i<MAX_CLUSTER_SLAVES; i++){
		if (cluster_proc[i] != -1){
			canditate_proc = get_proc_address(i);
			break;
		}
	}


	if (canditate_proc != -1){

		//puts("Task mapping for task "), puts(itoa(task_id)); puts(" maped at proc "); puts(itoh(canditate_proc)); puts("\n");

		return canditate_proc;
	}

	putsv("WARNING: no resources available in cluster to map task ", task_id);
	return -1;
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
int application_mapping(int cluster_id, int app_id){

	Application * app;
	Task *t;
	int proc_address;

	app = get_application_ptr(app_id);

	//puts("\napplication_mapping\n");

	for(int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		//putsv("Vai mapear task id: ", t->id);

		/*Map task*/
		proc_address = map_task(t->id, -1);

		if (proc_address == -1){

			return 0;

		} else {

			t->allocated_proc = proc_address;

			page_used(cluster_id, proc_address, t->id);

		}
	}

	puts("App have all its task mapped\n");

	return 1;
}

/**Selects a cluster to insert an application
 * \param GM_cluster_id cluster ID of the global manager processor
 * \param app_task_number Number of task of requered application
 * \return > 0 if mapping OK, -1 if there is not resources available
*/
int SearchCluster(int GM_cluster_id, int app_task_number) {

	int selected_cluster = -1;
	int freest_cluster = 0;

	for (int i=0; i<CLUSTER_NUMBER; i++){

		if (i == GM_cluster_id) continue;

		if (cluster_info[i].free_resources > freest_cluster){
			selected_cluster = i;
			freest_cluster = cluster_info[i].free_resources;
		}
	}

	if (cluster_info[GM_cluster_id].free_resources > freest_cluster){
		selected_cluster = GM_cluster_id;
	}

	return selected_cluster;
}

/** Searches following the diamond search paradigm. The search occurs only inside cluster
 * \param current_allocated_pe Current PE of the target task
 * \param last_proc The last proc returned by diamond_search function. 0 if is the first call
 * \return The selected processor address. -1 if not found
 */
int diamond_search(int current_allocated_pe, int last_proc, int task_utilization){

	int task_x, task_y, max_round, hop_count, max_tested_proc;//XY address of the current processor of the task
	int proc_x, proc_y, proc_count, proc_utilization, enable;
	int candidate_proc, candidate_util, free_pages;
	//int  proc_f;//APAGAR AO FINAL
	ClusterInfo * cinfo_ptr;

	cinfo_ptr = &cluster_info[clusterID];

	task_x = current_allocated_pe >> 8;
	task_y = current_allocated_pe & 0xFF;

	max_round = 0;
	hop_count = 0;
	proc_count = 0;

	if (last_proc > 0)
		enable = 0; //Enable is used to activate the utilization test
	else
		enable = 1;

	if (get_master_address(current_allocated_pe) != (cinfo_ptr->master_x << 8 | cinfo_ptr->master_y)){
		current_allocated_pe = (cinfo_ptr->master_x << 8 | cinfo_ptr->master_y);
		//puts("Warning!!! Parameter 'current_allocated_pe' not belongs to cluster scope, using cluster address by default\n");
	}

	//Sets the number of tested processor, if the task if outside of the cluster considers all cluster slave, otherwise, remove 1 proc, because the task is already mapped on it
	if (cinfo_ptr->xi <= task_x && cinfo_ptr->xf >= task_x && cinfo_ptr->yi <= task_y && cinfo_ptr->yf >= task_y)
		max_tested_proc = MAX_CLUSTER_SLAVES - 1;
	else
		max_tested_proc = MAX_CLUSTER_SLAVES;

	//Search candidate algorithm
	while(proc_count < max_tested_proc){

		//max_round count the number of processor of each round
		max_round+=4;
		hop_count++; //hop_count is used to position the proc_y at the top of the task's processor

		proc_x = task_x;
		proc_y = task_y + hop_count;

		candidate_proc = -1;//Init the variable to start the looping below
		candidate_util = 100;//Utilization is set to maximum

		//Walks for all processor of the round
		for(int r=0; r<max_round; r++){

			//Test if the current processor is out of the cluster dimension
			//To explore extra cluster search use this line
			//if (enable && 0 <= proc_x && (XDIMENSION-1) >= proc_x && 0 <= proc_y && (YDIMENSION-1) >= proc_y){ // Search all PEs
			if (enable && cinfo_ptr->xi <= proc_x && cinfo_ptr->xf >= proc_x && cinfo_ptr->yi <= proc_y && cinfo_ptr->yf >= proc_y && !(cinfo_ptr->master_x == proc_x && cinfo_ptr->master_y == proc_y)){

				//puts(itoa(proc_x)); putsv("x", proc_y);
				//Increment the number of valid processors visited
				proc_count++;

				//-------Heuristic to select the processor. Different heuristics must be implemented here. In this case, processors' utilization-------
				// {{0x205, 0x200},{0x305, 0x300},{0x003, 0x403}};
				//proc_f = proc_x << 8 | proc_y; //APAGAR AO FINAL
				//if (proc_f != 0x100 && proc_f != 0x205 && proc_f != 0x200 && proc_f != 0x305 && proc_f != 0x300 && proc_f != 0x003 && proc_f != 0x403){ //APAGAR AO FINAL

				free_pages = get_proc_free_pages(proc_x << 8 | proc_y);

				//Tests if the processor is not full
				if (free_pages > 0){

					//Extracts the processor utilization
					proc_utilization = get_proc_utilization(proc_x << 8 | proc_y);

					//test if the visited processor utilization is enough  to the task
					if ( free_pages == MAX_LOCAL_TASKS || ( (80-proc_utilization-task_utilization) > 0 && (candidate_proc == -1 || proc_utilization < candidate_util) ) ){
						//puts("Candidate found!!!! -> Proc = "); puts(itoh((proc_x << 8 | proc_y))); puts("\n");
						//putsv("Proc utilization = ", proc_utilization);
						//putsv("Task utilization = ", task_utilization);
						//return (proc_x << 8 | proc_y);
						candidate_proc = proc_x << 8 | proc_y;
						candidate_util = proc_utilization;
					}
				}

				//}//APAGAR AO FINAL
				//---------------------------------------------------------------------------------------------

			}

			//This if controls the starting point of the utilization test based on the last_proc address
			if (!enable && last_proc == (proc_x << 8 | proc_y))
				enable = 1;

			//These if walk over the processor addresses
			if (proc_y > task_y && proc_x <= task_x){ //up to left
					proc_y--;
					proc_x--;
			} else if (proc_y <= task_y && proc_x < task_x){//left to bottom
					proc_y--;
					proc_x++;
			} else if (proc_x >= task_x && proc_y < task_y){//bottom to right
					proc_y++;
					proc_x++;
			} else {//right to up
					proc_y++;
					proc_x--;
			}
		}

		if (candidate_proc != -1)
			return candidate_proc;

	}

	return -1;
}
