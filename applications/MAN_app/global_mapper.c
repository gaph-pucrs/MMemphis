/*
 * global_mapper.c
 *
 *  Created on: Jul 1, 2019
 *      Author: ruaro
 */
#include "common_include.h"
#include "mapping_includes/global_mapper/cluster_controller.h"


#define 		MAX_MA_TASKS			(MAX_SDN_TASKS+MAX_MAPPING_TASKS+MAX_QOS_TASKS+1) //+1 Due Global mapper
#define			MAPPING_TASK_REPO_ID	1
#define			SDN_TASK_REPO_ID		2
#define			QOS_TASK_REPO_ID		3

unsigned char	sdn_ID_offset;
unsigned char	map_ID_offset;
unsigned char	qos_ID_offset;


int ma_tasks_location[MAX_MA_TASKS];

/*Common Useful functions*/
void send_task_allocation_message(unsigned int task_repo_id, unsigned int real_task_id, unsigned int allocated_proc, unsigned int master_addr){

	unsigned int * message;

	message = get_message_slot();
	message[0] = APP_INJECTOR; //Destination
	message[1] = 5; //Packet size
	message[2] = APP_ALLOCATION_REQUEST; //Service
	message[3] = task_repo_id; //Repository task ID
	message[4] = master_addr; //Master address
	message[5] = allocated_proc;
	message[6] = real_task_id; //Real task id, when zero means to injector to ignore this flit. Otherwise, force the task ID to assume the specified ID value

	//Send message to Peripheral
	SendRaw(message, 7);

	while(!NoCSendFree());

	//Puts("Requesting task "); Puts(itoa(task_id)); Puts(" allocated at proc "); Puts(itoh(allocated_proc));
	//Puts(" belowing to master "); Puts(itoh(master_addr)); Puts("\n");

}

int get_local_mapper_index(unsigned int pos){
	int tx, ty;

	tx = pos >> 8;
	ty = pos & 0xFF;

	//Converts address in ID
	pos = (tx + ty*(XDIMENSION/MAPPING_XCLUSTER)); //PLus 1 because the global mapper uses ID 0

	return pos;
}

/**Returns the cluster addr 1x1, 1x0, based on the PE address and the cluster dimensions
 * Input
 *  - pe_addr: the address of a given PE on the system
 *  - x_cluster: the width in PE number of the cluster
 *  - y_cluster: the height in PE number of the cluster
 */
/*int get_cluster_addr_from_PE(unsigned int pe_addr, unsigned int x_cluster, unsigned int y_cluster){

	int tx, ty;

	tx = pe_addr >> 8;
	ty = pe_addr & 0xFF;

	tx = (int) (tx / x_cluster);
	ty = (int) (ty / y_cluster);

	return ((tx << 8) | ty);
}*/

void map_management_tasks(){

	int ma_task_index = 0;
	int cluster_index;
	int task_loc_aux;
	int x_aux, y_aux;
	int task_id;
	short int PE_usage[XDIMENSION][YDIMENSION];

	Puts("\nInitializing map_management_tasks\n\n");

	//Initializes the ma_tasks_location
	for(int i=0; i<MAX_MA_TASKS; i++){
		ma_tasks_location[i] = -1;
	}

	for(int y=0; y<YDIMENSION; y++){
		for(int x=0; x<XDIMENSION; x++){
			PE_usage[x][y] = MAX_LOCAL_TASKS;
		}
	}
	PE_usage[0][0] = 0;//Decrements because GM is allocated in this position

	ma_tasks_location[ma_task_index++] = 0;//The location of global mapper

	task_id = 1; //Initializes task ID with 1 since task ID 0 is the GM

	//**************************** Instantiate the Mapping MA tasks ********************************************
	map_ID_offset = 1;
	Puts("\nStarting MAPPING MA instantiation\n");
	for(int y=0; y<MAPPING_Y_CLUSTER_NUM; y++){
		for(int x=0; x<MAPPING_X_CLUSTER_NUM; x++){

			x_aux = (x*MAPPING_XCLUSTER);
			y_aux = (y*MAPPING_YCLUSTER);

			//Puts("PE "); Puts(itoh(x_aux<<8|y_aux)); Puts(" has free resources of "); Puts(itoa(PE_usage[x_aux][y_aux])); Puts("\n");
			//Heuristic that tries to find a PE to the MA task
			while(PE_usage[x_aux][y_aux] < 1){
				for(int xi = x_aux; xi < (x_aux+MAPPING_XCLUSTER); xi++){
					for(int yi = y_aux; yi < (y_aux+MAPPING_YCLUSTER); yi++){
						//Puts("Trying PE "); Puts(itoh(xi<<8|yi)); Puts("\n");
						if (PE_usage[xi][yi] > 0){
							x_aux = xi;
							y_aux = yi;
							xi = (x_aux+MAPPING_XCLUSTER); //Breaks the first loop
							break;
						}
					}
				}
			}

			task_loc_aux = (x_aux << 8) | y_aux;
			//Request to the injector to load the local_mapper inside the PEs
			//Task ID of local mappers are always 1
			send_task_allocation_message(MAPPING_TASK_REPO_ID, task_id, task_loc_aux, 0);

			//Remove one resource from the selected PE
			PE_usage[x_aux][y_aux]--;

			//Allocate cluster resources
			cluster_index = get_cluster_index_from_PE(task_loc_aux);
			allocate_cluster_resource(cluster_index, 1);

			Puts("Task mapping id "); Puts(itoa(task_id)); Puts(" allocated at proc ");
			Puts(itoh(task_loc_aux)); Puts("\n");

			//Compose the full information of task location and stores into the array
			task_loc_aux = (task_id << 16) | task_loc_aux;
			ma_tasks_location[ma_task_index++] = task_loc_aux;

			//Increments the task ID
			task_id++;

			//Decrements the total number of available resources of the system
			total_mpsoc_resources--;

		}
	}

	//**************************** Instantiate the SDN MA asks *****************************************
	Puts("\nStarting SDN MA instantiation\n");
	sdn_ID_offset = task_id;
	for(int y=0; y<SDN_Y_CLUSTER_NUM; y++){
		for(int x=0; x<SDN_X_CLUSTER_NUM; x++){

			x_aux = (x*SDN_XCLUSTER);
			y_aux = (y*SDN_YCLUSTER);

			//Heuristic that tries to find a PE to the MA task
			//Puts("PE "); Puts(itoh(x_aux<<8|y_aux)); Puts(" has free resources of "); Puts(itoa(PE_usage[x_aux][y_aux])); Puts("\n");
			while(PE_usage[x_aux][y_aux] < 1){
				for(int xi = x_aux; xi < (x_aux+SDN_XCLUSTER); xi++){
					for(int yi = y_aux; yi < (y_aux+SDN_YCLUSTER); yi++){
						//Puts("Trying PE "); Puts(itoh(xi<<8|yi)); Puts("\n");
						if (PE_usage[xi][yi] > 0){
							x_aux = xi;
							y_aux = yi;
							xi = (x_aux+SDN_XCLUSTER); //Breaks the first loop
							break;
						}
					}
				}
			}

			task_loc_aux = (x_aux << 8) | y_aux;

			//Request to the injector to load the local_mapper inside the PEs
			send_task_allocation_message(SDN_TASK_REPO_ID, task_id, task_loc_aux, 0);

			//Remove one resource from the selected PE
			PE_usage[x_aux][y_aux]--;

			//Allocate cluster resources
			cluster_index = get_cluster_index_from_PE(task_loc_aux);
			allocate_cluster_resource(cluster_index, 1);

			Puts("Task SDN id "); Puts(itoa(task_id)); Puts(" allocated at proc ");
			Puts(itoh(task_loc_aux)); Puts("\n");

			//Compose the full information of task location and stores into the array
			task_loc_aux = (task_id << 16) | task_loc_aux;
			ma_tasks_location[ma_task_index++] = task_loc_aux;

			//Increments the task ID
			task_id++;

			//Decrements the total number of available resources of the system
			total_mpsoc_resources--;
		}
	}


	Puts("\nStarting QoS Manager instantiation\n");
	qos_ID_offset = task_id;
	for(int y=0; y<QOS_Y_CLUSTER_NUM; y++){
		for(int x=0; x<QOS_X_CLUSTER_NUM; x++){

			x_aux = (x*QOS_XCLUSTER);
			y_aux = (y*QOS_YCLUSTER);

			//Heuristic that tries to find a PE to the MA task
			//Puts("PE "); Puts(itoh(x_aux<<8|y_aux)); Puts(" has free resources of "); Puts(itoa(PE_usage[x_aux][y_aux])); Puts("\n");
			while(PE_usage[x_aux][y_aux] < 1){
				for(int xi = x_aux; xi < (x_aux+QOS_XCLUSTER); xi++){
					for(int yi = y_aux; yi < (y_aux+QOS_YCLUSTER); yi++){
						//Puts("Trying PE "); Puts(itoh(xi<<8|yi)); Puts("\n");
						if (PE_usage[xi][yi] > 0){
							x_aux = xi;
							y_aux = yi;
							xi = (x_aux+QOS_XCLUSTER); //Breaks the first loop
							break;
						}
					}
				}
			}

			task_loc_aux = (x_aux << 8) | y_aux;

			//Request to the injector to load the local_mapper inside the PEs
			send_task_allocation_message(QOS_TASK_REPO_ID, task_id, task_loc_aux, 0);

			//Remove one resource from the selected PE
			PE_usage[x_aux][y_aux]--;

			//Allocate cluster resources
			cluster_index = get_cluster_index_from_PE(task_loc_aux);
			allocate_cluster_resource(cluster_index, 1);

			Puts("Task QoS id "); Puts(itoa(task_id)); Puts(" allocated at proc ");
			Puts(itoh(task_loc_aux)); Puts("\n");

			//Compose the full information of task location and stores into the array
			task_loc_aux = (task_id << 16) | task_loc_aux;
			ma_tasks_location[ma_task_index++] = task_loc_aux;

			//Increments the task ID
			task_id++;

			//Decrements the total number of available resources of the system
			total_mpsoc_resources--;
		}
	}
	/* Implement your new MA task here
	Puts("\nStarting MY MA instantiation\n");
	Please follow the patters of SDN tasks, you need to:
   	1. Create a macro MY_MA_TASK_REPO_ID. This macro must be in accordance with the appstart_debug.txt, i.e., as task local_mapper is the first in the list from the top to bottom, it receives the value of 1, SDN is the second, them it receives the value of 2, your MA task need to receive the appropriated value. Please check the number for every MA task since the scripts can scramble the sorting when you add a new task
	2. Set the ID_offset for your MA task based on the current task_id.
	3. Find a PE to map your MA task
	4. Request to the injector to load the MA task inside the PE
	5. Remove one resource from the selected PE
	6. Allocate cluster resources for your MA task
	7. Fills the ma_tasks_location with the ID and PE address (the location) of your new MA task
	8. Increment the task_id counter
	9. Decrement the total_mpsoc_resources resources
	 */


	putsv("\ntotal_mpsoc_resources: ", total_mpsoc_resources);

	//Adiciona a si mesmo na tabela da task location

	for(int i=0; i<MAX_MA_TASKS;i++){
		task_id = ma_tasks_location[i] >> 16;
		task_loc_aux = ma_tasks_location[i] & 0xFFFF;
		Puts("Task MA "); Puts(itoa(task_id)); Puts(" allocated at "); Puts(itoh(task_loc_aux)); Puts("\n");
		AddTaskLocation(task_id, task_loc_aux);
	}

	Puts("\nEnd map_management_tasks\n\n");
}

void handle_i_am_alive(unsigned int source_addr, unsigned int task_id){
	static unsigned int received_i_am_alive_counter = 0;
	unsigned int * message;
	unsigned int msg_size;
	unsigned int offset_id_aux;

	//Puts("\nhandle_i_am_alive from "); Puts(itoh(source_addr)); Puts(" task id "); Puts(itoa(task_id)); Puts("\n");

	received_i_am_alive_counter++;


	if (task_id >= map_ID_offset && task_id < (map_ID_offset+MAX_MAPPING_TASKS)){

		Puts("Mapping task ID: "); Puts(itoa(task_id)); Puts("\tinitialized at proc "); Puts(itoh(source_addr)); Puts("\n");

		offset_id_aux = (unsigned int) map_ID_offset;

	} else if (task_id >= sdn_ID_offset && task_id < (sdn_ID_offset+MAX_SDN_TASKS)){

		Puts("SDN task ID: "); Puts(itoa(task_id)); Puts("\tinitialized at proc "); Puts(itoh(source_addr)); Puts("\n");

		offset_id_aux = (unsigned int) sdn_ID_offset;

	} else if (task_id >= qos_ID_offset && task_id < (qos_ID_offset+MAX_QOS_TASKS)){

		Puts("QoS task ID: "); Puts(itoa(task_id)); Puts("\tinitialized at proc "); Puts(itoh(source_addr)); Puts("\n");

		offset_id_aux = (unsigned int) qos_ID_offset;

		/* Add the following code to setup the offset_id_aux according to your task
	} else if (task_id >= my_ID_offset && task_id < (my_ID_offset+MAX_MY_MA_TASKS)){
		offset_id_aux = (unsigned int) myMA_ID_offset;
	}
		 */

	} else {
		Puts("Task class not identified\n");
		while(1);
	}


	//Send the initialization packet
	msg_size = 0;
	message = get_message_slot();
	message[0] = INITIALIZE_MA_TASK;
	message[1] = task_id;
	message[2] = offset_id_aux;
	message[3] = MAX_MA_TASKS;

	msg_size = 4;
	for(int i=0; i<MAX_MA_TASKS;i++){
		message[msg_size++] = ma_tasks_location[i];
	}

	SendService(task_id, message, msg_size);

	while(!NoCSendFree());


	if (received_i_am_alive_counter == (MAX_MA_TASKS-1)){ //Minus 1 due the global mapper

		Puts("\nInitializing ALL MA TASKS complete\n");

		/*Sending MAPPING COMPLETE to APP INJECTOR*/
		message = get_message_slot();
		message[0] = APP_INJECTOR;
		message[1] = 2;//Payload should be 1, but is 2 in order to turn around a corner case in traffic monitor of Deloream for packets with payload 1
		message[2] = APP_MAPPING_COMPLETE;
		SendRaw(message, 4);

		while(!NoCSendFree());
	}

}

void handle_new_app_req(unsigned int app_cluster_id, unsigned int app_task_number){

	static unsigned int app_id_counter = 1;
	unsigned int cluster_loc;
	unsigned int * message;

	if (app_cluster_id == -1) //This -1 comes from scripts, its means that mapping is dynamic
		app_cluster_id = CLUSTER_NUMBER;

	if (app_task_number > total_mpsoc_resources){
		pending_app_req = app_task_number << 16 | app_cluster_id;
		Puts("Cluster full - return\n");
		return;
	}

	pending_app_req = 0;

	Puts("\n\n******** NEW_APP_REQ **********\n");
	//Puts(itoa(app_cluster_id));
	putsv("Task number: ", app_task_number);

	if (app_cluster_id == CLUSTER_NUMBER){
		Puts("dynamic\n");
		app_cluster_id = search_cluster(app_task_number);
	} else {
		Puts("static\n");
	}

	Puts("App mapped at cluster "); Puts(itoh(  clusters[app_cluster_id].x_pos << 8 | clusters[app_cluster_id].y_pos )); Puts("\n");
	Puts("Application ID: "); Puts(itoa(app_id_counter)); Puts("\n");

	//putsv("Global Master reserve application: ", num_app_tasks);
	//putsv("total_mpsoc_resources ", total_mpsoc_resources);

	putsv("\nCURRENT - total_mpsoc_resources: ", total_mpsoc_resources);
	total_mpsoc_resources -= app_task_number;
	putsv("AFTER - total_mpsoc_resources: ", total_mpsoc_resources);

	cluster_loc = (map_ID_offset+app_cluster_id) << 16 | GetTaskLocation(map_ID_offset+app_cluster_id);

	//Puts("Cluster loc "); Puts(itoa(cluster_loc)); Puts("\n");

	/*Sends packet to APP INJECTOR requesting repository to cluster app_cluster_id*/
	message = get_message_slot();
	message[0] = APP_INJECTOR;
	message[1] = 3; //Payload size
	message[2] = APP_REQ_ACK;
	message[3] = app_id_counter;
	message[4] = cluster_loc; //PLus 1 because as the global mapper is in ID 0 it shift all other IDs in more 1
	SendRaw(message, 5);

	app_id_counter++;
}

void handle_app_terminated(unsigned int *msg){
	unsigned int task_master_addr, index, app_task_number;
	int borrowed_cluster_index, original_cluster, original_cluster_index;

	putsv("\n --- > Handle APP terminated- app ID: ", msg[1]);
	app_task_number = msg[2];
	original_cluster = msg[4];

	//Puts("original master pos: "); Puts(itoh(original_cluster)); Puts("\n");

	original_cluster_index = get_local_mapper_index(original_cluster);

	//Puts("original master index: "); Puts(itoa(original_cluster_index)); Puts("\n");

	index = 4;

	for (int i=0; i<app_task_number; i++){
		task_master_addr = msg[index++];

		//Puts("Terminated task id "); Puts(itoa(msg[1] << 8 | i)); Puts(" with master pos "); Puts(itoh(task_master_addr)); Puts("\n");

		if (task_master_addr != original_cluster){

			//Puts("Remove borrowed\n");
			borrowed_cluster_index = get_local_mapper_index(task_master_addr);

			release_cluster_resources(borrowed_cluster_index, 1);

		} else {
			//Puts("Remove original\n");
			release_cluster_resources(original_cluster_index, 1);
		}
	}
	total_mpsoc_resources += app_task_number;

	putsv("App terminated, total_mpsoc_resources: ", total_mpsoc_resources);

	/* terminated_app_count++;
	 * if (terminated_app_count == APP_NUMBER){
		Puts("FINISH ");Puts(itoa(GetTick)); Puts("\n");
		MemoryWrite(END_SIM,1);
	}*/
	//pending_app_req = app_task_number << 16 | app_cluster_id;
	if (pending_app_req){
		Puts("Pending APP to req TRUE\n");
		app_task_number = pending_app_req >> 16;
		original_cluster_index = pending_app_req & 0xFF;

		handle_new_app_req(original_cluster_index, app_task_number);

	}
}

void handle_app_allocated(unsigned int * msg){
	unsigned int cluster_index, index, app_task_number;
	unsigned int * message;

	putsv("\n******************************\nReceive APP_ALLOCATED for app ", msg[1]);
	app_task_number = msg[2];

	index = 3;
	for(int i=0; i<app_task_number; i++){
		cluster_index = get_local_mapper_index(msg[index++]);
		Puts("Task "); Puts(itoa(msg[1] << 8 | i)); Puts(" to cluster "); Puts(itoh(msg[index-1])); Puts("\n");
		allocate_cluster_resource(cluster_index, 1);
	}

	message = get_message_slot();
	message[0] = APP_INJECTOR;
	message[1] = 2; //Payload should be 1, but is 2 in order to turn around a corner case in traffic monitor of Deloream for packets with payload 1
	message[2] = APP_MAPPING_COMPLETE; //Service
	SendRaw(message, 4);
	Puts("Sent APP_MAPPING_COMPLETE\n");

}

void handle_message(unsigned int * data_msg){

	switch (data_msg[0]) {
		case INIT_I_AM_ALIVE:
			handle_i_am_alive(data_msg[1], data_msg[2]);
			break;
		case NEW_APP_REQ:
			handle_new_app_req(data_msg[1], data_msg[2]);
			break;
		case APP_ALLOCATED:
			handle_app_allocated(data_msg);
			break;
		case APP_TERMINATED:
			handle_app_terminated(data_msg);
			break;
		case PATH_CONNECTION_ACK:
			Puts("ACK received from SDN controller at time: "); Puts(itoa(GetTick())); Puts("\n");
			break;
		default:
			Puts("Error message unknown\n");
			for(;;);
			break;
	}
}

void main(){

	RequestServiceMode();

	Puts("Initializing Global Mapper\n");
	init_message_slots();
	intilize_clusters();
	map_management_tasks();
	//init_sdn_tasks();
	//init_dvfs_tasks();
	//init_monitoring_tasks();
	//init_decision_tasks();
	//init_adaptation_tasks();

	unsigned int data_message[MAX_MANAG_MSG_SIZE];

	for(;;){

		ReceiveService(data_message);
		handle_message(data_message);

	}

	exit();

}



