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
#include "mapping_includes/local_mapper/circuit_switching.h"
#include "mapping_includes/local_mapper/resoucer_controller.h"

//SO USADO PARA RESULTADOS
unsigned int admission_time = 0;

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

	//Puts("Cluster position: "); Puts(itoh(cluster_position)); Puts("\n");
	//Puts("Cluster x offset: "); Puts(itoa(cluster_x_offset)); Puts("\n");
	//Puts("Cluster y offset: "); Puts(itoa(cluster_y_offset)); Puts("\n");

	//Initialize processors and global variable
	init_procesors();
	pending_app_control = 0;

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


void request_application2(Application *app){

	Task *t;
	int index_counter;
	unsigned int master_allocation, msg_size;
	unsigned int * message;

	//Puts("\nRequest APP\n");

	index_counter = 0;
	msg_size = 0;

	//Master addr composition used by the APP_INJECTOR to send the task
	master_allocation = (my_task_ID << 16) | GetNetAddress();

	msg_size = 0;
	message = get_message_slot();
	message[msg_size++] = APP_MAPPING_COMPLETE;
	message[msg_size++] = app->app_ID;
	message[msg_size++] = app->tasks_number;
	message[msg_size++] = app->is_secure;

	//Next flits are a set: {task ID, master_allocation, alooc_proc, cluster_index
	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		while (t->allocated_proc == -1){
			Puts("ERROR task id not allocated: "); Puts(itoa(t->id)); Puts("\n");
		}

		message[msg_size++] = t->id;
		message[msg_size++] = master_allocation;
		message[msg_size++] = t->allocated_proc;
		if (t->borrowed_master == -1){
			message[msg_size++] = cluster_position;
		} else {
			message[msg_size++] = t->borrowed_master;
		}

	}

	admission_time = GetTick() - admission_time;
	putsv("SDN PATHs complete - phase (d): ", admission_time);

#if LM_DEBUG
	Puts("SDN path completed! Send APP_MAPPING_COMPLETE\n");
#endif
	admission_time = GetTick();

	SendService(global_task_ID, message, msg_size);


	//Waits the packet to be sent
	//while(!NoCSendFree());

}



/** Requests a new application to the global master kernel
 *  \param app Application to be requested
 */
int request_application(Application *app){

	Task *t;
	int index_counter;
	unsigned int master_addr, msg_size;
	unsigned int * message;

	//Puts("\nRequest APP\n");

	index_counter = 0;

	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		while (t->allocated_proc == -1){
			Puts("ERROR task id not allocated: "); Puts(itoa(t->id)); Puts("\n");
		}

		if (t->status == TASK_FREE){

			t->status = REQUESTED;

			master_addr = (my_task_ID << 16) | GetNetAddress();

			msg_size = 0;
			message = get_message_slot();
			message[msg_size++] = APP_INJECTOR; //Destination
			message[msg_size++] = 5; //Packet size
			message[msg_size++] = APP_ALLOCATION_REQUEST; //Service
			message[msg_size++] = t->id; //Repository task ID
			message[msg_size++] = master_addr; //Master address
			message[msg_size++] = t->allocated_proc;
			message[msg_size++] = 0; //Real task id, when zero means to injector to ignore this flit. Otherwise, force the task ID to assume the specified ID value
			//Send message to Peripheral
			SendRaw(message, msg_size);
			//Waits the packet to be sent
			while(!NoCSendFree());

			putsv("Master addr: ", master_addr);

			Puts("\nRequesting task "); Puts(itoa(t->id)); Puts(" at proc "); Puts(itoh(t->allocated_proc));
			putsv(" at time ", GetTick());

			return 1;
		}
	}

	Puts("All task requested!\n");

	return 0;

}

/** Handles a pending application. A pending application it the one which there is some task waiting to be mapped by reclustering protocol
 */
void handle_pending_application(){

	Application *app = 0;
	int complete = 0;

	//Puts("Handle next application \n");

	/*Selects an application pending to be mapped due reclustering*/
	app = get_next_pending_app();

	//putsv("Pending app ID: ", app->app_ID);

	switch (app->status) {

		case WAITING_MAPPING:

			//Avoids to map a secure app without have the cs_utilization matrix updated
			if (app->is_secure && cs_utilization_updated != CS_UTIL_UPDATED){
				if (cs_utilization_updated == CS_UTIL_OUTDATED)
					request_cs_util_update();
				break;
			}

			complete = application_mapping(app);

			if (complete){

				if (app->is_secure){

#if LM_DEBUG
					Puts("App WAITING_CIRCUIT_SWITCHING\n");
#endif

					app->status = WAITING_CIRCUIT_SWITCHING;

				} else {

#if LM_DEBUG
					Puts("App READY_TO_LOAD\n");
#endif

					app->status = READY_TO_LOAD;
				}

			} else {

#if LM_DEBUG
				Puts("Application WAITING_RECLUSTERING\n");
#endif

				app->status = WAITING_RECLUSTERING;

			}

			break;

		case WAITING_RECLUSTERING:

			//This line fires the reclustering protocol
			complete = reclustering_next_task(app);

			if (complete){ //If reclustering is complete for that application

				Puts("Reclustering complete - ");

				if (app->is_secure){

					app->status = WAITING_CIRCUIT_SWITCHING;

					Puts("app WAITING_CIRCUIT_SWITCHING\n");

				} else {

					Puts("app READY_TO_LOAD\n");

					app->status = READY_TO_LOAD;
				}
			}

			break;

		case WAITING_CIRCUIT_SWITCHING:

//#if LM_DEBUG
			admission_time = GetTick() - admission_time;
			putsv("End MAPPING - phase (c): ", admission_time);
			Puts("Init SDN establishment protocol...\n");
//#endif
			admission_time = GetTick();

			request_connection(app);

			break;

		case READY_TO_LOAD:

			pending_app_control--;

			//putsv("pending_app_control: ", pending_app_control);

			request_application2(app);

			app->status = RUNNING;

			/*if (cs_utilization_updated == CS_UTIL_OUTDATED){
				request_cs_util_update();
			}*/

			break;
	}

}

void handle_new_app(unsigned int * msg){

	Application * application;
	unsigned int * ref_address;
	unsigned int app_ID;

//#if LM_DEBUG
	Puts("\nHandle new APP DESCRIPTOR, app ID "); Puts(itoa(msg[1])); Puts("\n");
//#endif
	admission_time = GetTick();

	app_ID = msg[1];
	ref_address = &msg[3];

	/*for(int i=0; i<msg[2]; i++){
		Puts(itoh(ref_address[i])); Puts("\n");
	}*/

	//Creates a new app by reading from ref_address
	application = read_and_create_application(app_ID, ref_address);

	//This variable control the pending apps, i.e., apps that are already handled but are not in execution
	pending_app_control++;

	handle_pending_application();

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

		//putsv("Sent TASK_RELEASE to task ", app->tasks[i].id);
		//puts(" in proc "); puts(itoh(p->header)); puts("\n----\n");
	}

	Puts("TASK_RELEASE sent! APP_RUNNING!!\n");

	app->status = RUNNING;

}

void send_app_allocated(Application * app_ptr){
	unsigned int msg_size;
	unsigned int * message;
	Task * task_ptr;

	/*********** Send APP_ALLOCTED to global************/
	message = get_message_slot();
	message[0] = APP_ALLOCATED;
	message[1] = app_ptr->app_ID;
	SendService(global_task_ID, message, 2);

//#if LM_DEBUG
	admission_time = GetTick() - admission_time;
	Puts("APP_ALLOCATED sent - phase (e): "); Puts(itoa(admission_time)); Puts("\n");
	admission_time = GetTick();
	//putsv("APP_ALLOCATED sent ", app_ptr->app_ID);
//#endif
	/****************************************************/
}

void handle_task_allocated(unsigned int task_id, unsigned int security_allocation){

	unsigned int app_id, allocated_tasks;
	Application * app_ptr;

#if LM_DEBUG
	putsv("-> TASK ALLOCATED from task ", task_id);

	if (security_allocation)
		Puts("\ttrusted task\n");
	else
		Puts("\tmalicious task - canceling the app allocation\n");
#endif

	app_id = task_id >> 8;

	app_ptr = get_application_ptr(app_id);

	allocated_tasks = set_task_allocated(app_ptr, task_id);

	if (allocated_tasks == app_ptr->tasks_number){

		//If the App. is secure first configures CS at tasks kernel for all CTPs
		if (app_ptr->is_secure && initial_CS_setup_protocol(app_ptr, 0, 0) ){
			//This means that the protocol was started and it will resume the execution when a ack packet is received
			return;
		}

		//Otherwise the Manager can realease the app to run

		send_app_allocated(app_ptr);

		//Puts("\nSEND TASK RELEASE\n\n");
		/*Send the TASK RELEASE to all tasks begin its execution*/
		send_task_release(app_ptr);
	}
}

void handle_task_terminated(unsigned int task_id, unsigned int master_addr){
	Application * app_ptr;
	Task * task_ptr;
	ConsumerTask * ct;
	unsigned int master_address;
	unsigned int master_id;
	unsigned int * message;
	unsigned int msg_size;
	unsigned int target_addr;

	putsv("TASK_TERMINATED received from task: ", task_id);

	master_address = master_addr & 0xFFFF;
	master_id = master_addr >> 16;

	app_ptr = get_application_ptr(task_id >> 8);

	set_task_terminated(app_ptr, task_id);

	task_ptr =  get_task_ptr(app_ptr, task_id);

	//Release all CS connections
	for(int i=0; i<task_ptr->consumers_number; i++){
		ct = &task_ptr->consumer[i];

		if (ct->id != -1 && ct->subnet != PS_SUBNET){

			target_addr = get_task_location(ct->id);

			Puts("CS Releasing to "); Puts(itoa(task_ptr->id)); putsv(" -> ", ct->id);
			request_SDN_path(task_ptr->allocated_proc, target_addr, ct->subnet);

			//Every time that a path release happens the utilization becomes
			//outdated
			cs_utilization_updated = CS_UTIL_OUTDATED;
		}
	}


	if (task_ptr->borrowed_master != -1){
		//page_released(task_ptr->allocated_proc, task_id);
		//Puts("Task is local, page released\n");
	//} else {
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

			task_ptr = &app_ptr->tasks[i];

			//Puts("Task "); Puts(itoa(app_ptr->tasks[i].id));
			if (task_ptr->borrowed_master != -1){
				message[msg_size++] = task_ptr->borrowed_master;
				//Puts(" master borrowed "); Puts(itoh(task_ptr->borrowed_master)); Puts("\n");
			} else {
				message[msg_size++] = cluster_position;
				page_released(task_ptr->allocated_proc, task_ptr->id);
				//Puts(" master local "); Puts(itoh(cluster_position)); Puts("\n");
			}
		}
		/*As the message is being sent to global master is being used the SendService( instead send(*/
		SendService(global_task_ID, message, msg_size);

		remove_application(app_ptr);

		Puts("App terminated!\n\n");

		//Request to SDN controller to update the cs utilization table
		/*if (cs_utilization_updated == CS_UTIL_OUTDATED){
			request_cs_util_update();
		}*/
	}
}

void handle_set_initial_cs_ack(unsigned int producer_id, unsigned int consumer_id){
	Application * app_ptr;

	app_ptr = get_application_ptr(consumer_id >> 8);

	while (!app_ptr) Puts("ERROR: app id invalid\n");

	//putsv("ACK received\nproducer id: ", producer_id);
	//putsv("consumer id: ", consumer_id);

	if (initial_CS_setup_protocol(app_ptr, producer_id, consumer_id) == 0){

		send_app_allocated(app_ptr);

		send_task_release(app_ptr);
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
			handle_task_allocated(data_msg[1], data_msg[2]);
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
		case PATH_CONNECTION_ACK:
			//Commented because I am testing the secure DMNI, remove the comment after
			handle_SDN_ack(data_msg);
			break;

		case SET_INITIAL_CS_ACK:

			handle_set_initial_cs_ack(data_msg[1], data_msg[2]);
			break;

		case CS_UTILIZATION_RESPONSE:

			handle_cs_utilization_response(data_msg);
			break;

		default:
			while(1){
				Puts("Error service message unknown: "); Puts(itoh(*(data_msg++))); Puts("\n");
			}
			break;
	}
}

void main(){

	Echo("Initializing Local Mapper\n");
	RequestServiceMode();
	init_message_slots();
	initialize_applications();
	init_reclustering();
	initialize_MA_task();
	Puts("My net address: "); Puts(itoh(net_address)); Puts("\n");

	//Worst case message size == (MAX_TASKS_APP*TASK_DESCRIPTOR_SIZE)+1
	unsigned int data_message[MAX_TASKS_APP*TASK_DESCRIPTOR_SIZE+100]; //usando +100 para garantir

	for(;;){

		//Não gerar nova demanda se algo ainda nao foi consumido na NoC

		if (IncomingPacket()){
			ReceiveService(data_message);
			handle_message(data_message);
			continue;
		}

		//if (is_reclustering_NOT_active() && is_CS_not_active() && pending_app_control && !IncomingPacket() && NoCSendFree()){

		//If there some app waiting, there is no incomming packet and the NI send is free then:
		if (pending_app_control && NoCSendFree() && is_reclustering_NOT_active() && is_CS_not_active() && cs_utilization_updated != CS_UTIL_REQUESTED && !IncomingPacket() && NoCSendFree()){

			handle_pending_application();

		} else {

			ReceiveService(data_message);
			handle_message(data_message);
		}

	}

	exit();
}


