/*
 * local_mapper.c
 *
 *  Created on: Jul 2, 2019
 *      Author: ruaro
 */
#include "common_include.h"
#include "local_mapper_modules/resoucer_controller.h"
#include "local_mapper_modules/application.h"
#include "local_mapper_modules/processors.h"
#include "local_mapper_modules/reclustering.h"


void initialize(){

	AddTaskLocation(global_mapper_id,global_mapper_id);

	unsigned int * message = get_message_slot();

	message[0] = INIT_I_AM_ALIVE;		//Task Service
	message[1] = GetNetAddress();	//Task Address

	Puts("Sending I AM ALIVE to global mapper\n");
	SendService(global_mapper_id, message, 2);
}

void initialize_ids(unsigned int * msg){

	int msg_index;
	unsigned int id, loc;
	unsigned int my_id;

	cluster_position =  msg[1];
	cluster_x_offset = (cluster_position >> 8) * XCLUSTER;
	cluster_y_offset = (cluster_position & 0xFF) * YCLUSTER;

	init_procesors();

	my_id = position_to_ID(cluster_position);

	//Registers the page using to the own task
	page_used(GetNetAddress(), my_id);

	if (cluster_position == 0x000){
		//Sets page used for the global manager
		page_used(0, 0);
	}

	msg_index = 2;


	for(int i=0; i<=CLUSTER_NUMBER; i++){
		id = msg[msg_index++];
		loc = msg[msg_index++];
		putsv("Addint task ", id);
		AddTaskLocation(id, loc);
	}

	SetMyID(my_id);
	Puts("Local mapper initialized by global mapper, address = "); Puts(itoh(cluster_position)); Puts("\n");
	Puts("Cluster position: "); Puts(itoh(cluster_position)); Puts("\n");
	Puts("Cluster x offset: "); Puts(itoa(cluster_x_offset)); Puts("\n");
	Puts("Cluster y offset: "); Puts(itoa(cluster_y_offset)); Puts("\n");
}

/** Assembles and sends a APP_ALLOCATION_REQUEST packet to the global master
 *  \param app The Application instance
 *  \param task_info An array containing relevant task informations
 */
void send_app_allocation_request(Application * app_ptr){

	Task * task_ptr;

	for(int task = 0; task < app_ptr->tasks_number; task++){

		task_ptr = &app_ptr->tasks[task];

		send_task_allocation_message(task_ptr->id, task_ptr->allocated_proc, cluster_position);
	}

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


void handle_message(unsigned int * data_msg){

	switch (data_msg[0]) {
		case INIT_LOCAL:
			initialize_ids(data_msg);
			break;
		case NEW_APP:
			handle_new_app(data_msg);
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
	init_message_slots();
	initialize_applications();
	init_reclustering();
	initialize();

	unsigned int data_message[MAX_MAPPING_MSG];

	for(;;){

		ReceiveService(data_message);
		handle_message(data_message);

		if (is_reclustering_NOT_active() && pending_app_to_map && !IncomingPacket() && NoCSendFree()){
			handle_pending_application();
		}

	}

	exit();
}


