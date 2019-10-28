/*
 * local_mapper.c
 *
 *  Created on: Jul 2, 2019
 *      Author: ruaro
 */
#include "common_include.h"
#include "mapping_includes/local_mapper/application.h"
#include "mapping_includes/local_mapper/processors.h"
#include "mapping_includes/local_mapper/reclustering.h"
#include "mapping_includes/local_mapper/resoucer_controller.h"

void initialize_local_mapper(unsigned int * msg){

	unsigned int max_ma_tasks;
	unsigned int task_id, proc_addr, id_offset;
	unsigned int x_aux, y_aux;

	task_id = msg[1];
	id_offset = msg[2];

	my_task_ID = task_id;

	Puts("\n ************ Initialize Local Mapper *********** \n");
	Puts("Task ID: "); Puts(itoa(task_id)); Puts("\n");
	Puts("Offset ID: "); Puts(itoa(id_offset)); Puts("\n");
	Puts("MAX_MA_TASKS: "); Puts(itoa(msg[3])); Puts("\n");

	cluster_position = conv_task_ID_to_cluster_addr(task_id, id_offset, MAPPING_X_CLUSTER_NUM);
	cluster_x_offset = (cluster_position >> 8) * MAPPING_XCLUSTER;
	cluster_y_offset = (cluster_position & 0xFF) * MAPPING_YCLUSTER;

	Puts("Cluster position: "); Puts(itoh(cluster_position)); Puts("\n");
	Puts("Cluster x offset: "); Puts(itoa(cluster_x_offset)); Puts("\n");
	Puts("Cluster y offset: "); Puts(itoa(cluster_y_offset)); Puts("\n");

	//Initialize processors and global variable
	init_procesors();
	pending_app_to_map = 0;

	max_ma_tasks = msg[3];

	//Set the proper location of all MA tasks
	for(int i=0; i<max_ma_tasks; i++){
		task_id = msg[i+4] >> 16;
		proc_addr = msg[i+4] & 0xFFFF;
		//Puts("Task MA "); Puts(itoa(task_id)); Puts(" allocated at "); Puts(itoh(proc_addr)); Puts("\n");
		AddTaskLocation(task_id, proc_addr);

		//Gets the cluster of the processor and if the cluster is the current cluster, update the page used
		x_aux = proc_addr >> 8;
		y_aux = proc_addr & 0xFF;

		x_aux = x_aux / MAPPING_XCLUSTER;
		y_aux = y_aux / MAPPING_YCLUSTER;

		if ( ((x_aux << 8) | y_aux) == cluster_position){
			page_used(proc_addr, task_id);
		}
	}

	//Puts("Cluster addr: "); Puts(itoh(conv_task_ID_to_cluster_addr(task_id, id_offset, SDN_X_CLUSTER_NUM))); Puts("\n");

	init_generic_send_comm(id_offset, MAPPING_XCLUSTER);

	Puts("Local Mapper initialized!!!!!!\n\n");
}

/** Assembles and sends a APP_ALLOCATION_REQUEST packet to the global master
 *  \param app The Application instance
 *  \param task_info An array containing relevant task informations
 */
void send_app_allocation_request(Application * app_ptr){

	Task * task_ptr;
	unsigned int master_addr, msg_size;
	unsigned int * message;


	master_addr = (my_task_ID << 16) | net_address;

	message = get_message_slot();
	msg_size = 0;
	//Asembles the packet
	for(int task = 0; task < app_ptr->tasks_number; task++){

		task_ptr = &app_ptr->tasks[task];

		while(!NoCSendFree());

		message[msg_size++] = APP_INJECTOR; //Destination
		message[msg_size++] = 5; //Packet size
		message[msg_size++] = APP_ALLOCATION_REQUEST; //Service
		message[msg_size++] = task_ptr->id; //Repository task ID
		message[msg_size++] = master_addr; //Master address
		message[msg_size++] = task_ptr->allocated_proc;
		message[msg_size++] = 0; //Real task id, when zero means to injector to ignore this flit. Otherwise, force the task ID to assume the specified ID value

	}
	//Send message to Peripheral
	SendRaw(message, msg_size);

}

/** Requests a new application to the global master kernel
 *  \param app Application to be requested
 */
void request_application(Application *app){

	Task *t;
	int index_counter;

	Puts("\nRequest APP\n");

	pending_app_to_map--;

	index_counter = 0;

	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		if (t->allocated_proc == -1){
			Puts("ERROR task id not allocated: "); Puts(itoa(t->id)); Puts("\n");
			while(1);
		}

		t->status = REQUESTED;

	}

	//Sends app allocation to Injector
	send_app_allocation_request(app);

}

/** Handles a pending application. A pending application it the one which there is some task waiting to be mapped by reclustering protocol
 */
void handle_pending_application(){

	Application *app = 0;
	int request_app = 0;

	Puts("Handle next application \n");

	/*Selects an application pending to be mapped due reclustering*/
	app = get_next_pending_app();

	//This line fires the reclustering protocol
	request_app = reclustering_next_task(app);

	if (request_app){

		app->status = READY_TO_LOAD;

		request_application(app);
	}
}

void handle_new_app(unsigned int * msg){

	Application * application;
	unsigned int * ref_address;
	unsigned int app_ID;
	int mapping_completed = 0;


	Puts("Handle new APP DESCRIPTOR from APP INJECTOR\n");
	Puts("App ID: "); Puts(itoa(msg[1])); Puts("\n");

	app_ID = msg[1];
	ref_address = &msg[3];

	/*for(int i=0; i<msg[2]; i++){
		Puts(itoh(ref_address[i])); Puts("\n");
	}*/

	//Creates a new app by reading from ref_address
	application = read_and_create_application(app_ID, ref_address);

	pending_app_to_map++;

	mapping_completed = application_mapping(application->app_ID);

	if (mapping_completed){

		application->status = READY_TO_LOAD;

		request_application(application);

	} else {

		Puts("Application waiting reclustering\n");

		application->status = WAITING_RECLUSTERING;

	}

}

/** Assembles and sends a TASK_RELEASE packet to a slave kernel
 *  \param app The Application instance
 */
void send_task_release(Application * app){

	unsigned int * message;
	unsigned int msg_size = CONSTANT_PKT_SIZE;

	message = get_message_slot();

	for (int i =0; i<app->tasks_number; i++){

		message[msg_size++] = app->tasks[i].allocated_proc;
		//Puts("Send task release"); Puts(itoa(app->app_ID << 8 | i)); Puts(" loc "); Puts(itoh(app->tasks[i].allocated_proc)); Puts("\n");
	}

	//putsv("MEssage size: ", msg_size);

	for (int i =0; i<app->tasks_number; i++){

		while(!NoCSendFree());

		message[0] = app->tasks[i].allocated_proc;
		message[1] = msg_size - 2;
		message[2] = TASK_RELEASE;
		message[3] = app->tasks[i].id; //p->task_ID
		message[8] = app->tasks_number; //p->app_task_number
		message[9] = app->tasks[i].data_size; //p->data_size
		//message[10] = ignored
		message[11] = app->tasks[i].bss_size; //p->bss_size

		if (app->tasks[i].allocated_proc == net_address){
			SetTaskRelease(message, msg_size);
		} else {
			SendRaw(message, msg_size);
		}


		app->tasks[i].status = TASK_RUNNING;

		//putsv("\n -> send TASK_RELEASE to task ", app->tasks[i].id);
		//puts(" in proc "); puts(itoh(p->header)); puts("\n----\n");
	}

	app->status = RUNNING;

}

void handle_task_allocated(unsigned int task_id){

	unsigned int app_id, allocated_tasks, msg_size;
	Application * app_ptr;
	Task * task_ptr;
	unsigned int * message;

	putsv("\n -> TASK ALLOCATED from task ", task_id);

	app_id = task_id >> 8;

	app_ptr = get_application_ptr(app_id);

	allocated_tasks = set_task_allocated(app_ptr, task_id);

	if (allocated_tasks == app_ptr->tasks_number){

		/*********** Send APP_ALLOCTED to global************/
		message = get_message_slot();
		message[0] = APP_ALLOCATED;
		message[1] = app_ptr->app_ID;
		message[2] = app_ptr->tasks_number;
		msg_size = 3;
		for(int task = 0; task < app_ptr->tasks_number; task++){
			task_ptr = &app_ptr->tasks[task];

			if (task_ptr->borrowed_master == -1){
				message[msg_size++] = cluster_position;
			} else {
				message[msg_size++] = task_ptr->borrowed_master;
			}
			//Puts("Task "); Puts(itoa(task_ptr->id)); Puts(" to cluster "); Puts(itoh(message[msg_size-1])); Puts("\n");
		}
		SendService(global_task_ID, message, msg_size);
		Puts("APP_ALLOCATED sent\n\n");
		/****************************************************/


		//Puts("\nSEND TASK RELEASE\n\n");
		/*Send the TASK RELEASE to all tasks begin its execution*/
		send_task_release(app_ptr);
	}
}

void handle_task_terminated(unsigned int task_id, unsigned int master_addr){
	Application * app_ptr;
	Task * task_ptr;
	unsigned int master_address;
	unsigned int master_id;
	unsigned int * message;
	unsigned int msg_size;

	putsv("TASK_TERMINATED received from task: ", task_id);

	master_address = master_addr & 0xFFFF;
	master_id = master_addr >> 16;

	app_ptr = get_application_ptr(task_id >> 8);

	set_task_terminated(app_ptr, task_id);

	task_ptr =  get_task_ptr(app_ptr, task_id);

	if (task_ptr->borrowed_master == -1){
		page_released(task_ptr->allocated_proc, task_id);
		//Puts("Task is local, page released\n");
	} else {
		message = get_message_slot();
		message[0] = TASK_TERMINATED_OTHER_CLUSTER;
		message[1] = task_ptr->allocated_proc;
		message[2] = task_id;
		send(task_ptr->borrowed_master, message, 3);
		Puts("Sending TASK_TERMINATED_OTHER_CLUSTER to "); Puts(itoh(task_ptr->borrowed_master)); Puts("\n");
	}

	//Test if is necessary to terminate the app
	if (app_ptr->terminated_tasks == app_ptr->tasks_number){

		/*Sends APP TERMINATOR to global mapper*/
		message = get_message_slot();
		message[0] = APP_TERMINATED;
		message[1] = app_ptr->app_ID;
		message[2] = app_ptr->tasks_number;
		message[3] = cluster_position;
		msg_size = 4;

		for (int i=0; i<app_ptr->tasks_number; i++){

			//Puts("Task "); Puts(itoa(app_ptr->tasks[i].id));
			if (app_ptr->tasks[i].borrowed_master != -1){
				message[msg_size++] = app_ptr->tasks[i].borrowed_master;
				//Puts(" master borrowed "); Puts(itoh(app_ptr->tasks[i].borrowed_master)); Puts("\n");
			} else {
				message[msg_size++] = cluster_position;
				//Puts(" master local "); Puts(itoh(cluster_position)); Puts("\n");
			}
		}
		/*As the message is being sent to global master is being used the SendService( instead send(*/
		SendService(global_task_ID, message, msg_size);

		remove_application(app_ptr);

		Puts("App terminated!\n\n");
	}
}

void handle_message(unsigned int * data_msg){

	switch (data_msg[0]) {
		case INITIALIZE_MA_TASK:
			initialize_local_mapper(data_msg);
			break;
		case NEW_APP:
			handle_new_app(data_msg);
			break;
		case TASK_ALLOCATED:
			handle_task_allocated(data_msg[1]);
			break;
		case TASK_TERMINATED:
			handle_task_terminated(data_msg[1], data_msg[2]);
			break;
		case TASK_TERMINATED_OTHER_CLUSTER:
			//			  proc_addr,    task_id
			page_released(data_msg[1], data_msg[2]);
			break;

		case LOAN_PROCESSOR_REQUEST:
		case LOAN_PROCESSOR_DELIVERY:
		case LOAN_PROCESSOR_RELEASE:

			handle_reclustering(data_msg);

			break;
		default:
			Puts("Error message unknown\n");
			for(;;);
			break;
	}
}

void main(){

	Echo("Initializing Local Mapper\n");
	RequestServiceMode();
	net_address = GetNetAddress();
	init_message_slots();
	initialize_applications();
	init_reclustering();
	initialize_MA_task();

	unsigned int data_message[MAX_MANAG_MSG_SIZE];

	for(;;){
		ReceiveService(data_message);
		handle_message(data_message);

		if (is_reclustering_NOT_active() && pending_app_to_map && !IncomingPacket() && NoCSendFree()){
			handle_pending_application();
		}

	}

	exit();
}


