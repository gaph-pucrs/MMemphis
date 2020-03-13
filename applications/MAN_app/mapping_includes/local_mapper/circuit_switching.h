/*
 * circuit_switching.h
 *
 *  Created on: 8 de nov de 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_
#define APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_

#include "application.h"

Application   * app_ctp_ptr = 0;
Task		  * producer_task_ptr = 0;
ConsumerTask  * consumer_ptr = 0;

int request_connection(Application * app){
 //1. Organize the CTPs
 //2. Send they to the SDN controller
 //3. Set app waiting reclustering
	ConsumerTask * ct;
	Task * t;
	unsigned int source_pe, target_pe;

	Puts("\n---- Request connection\n");

	app_ctp_ptr = 0;
	producer_task_ptr = 0;
	consumer_ptr = 0;

	for(int i=0; i<app->tasks_number; i++){
		t = &app->tasks[i];
		source_pe = t->allocated_proc;

		for(int k=0; k<t->consumers_number; k++){
			ct = &t->consumer[k];

			if (ct->id != -1 && ct->subnet == PS_SUBNET){
				target_pe = get_task_location(ct->id);

				Puts("ctp found: ["); Puts(itoa(t->id)); Puts("] -> ["); Puts(itoa(ct->id)); Puts("]\n");

				app_ctp_ptr = app;
				producer_task_ptr = t;
				consumer_ptr = ct;

				i = app->tasks_number; //Break the 1st for
				break; //Break the current for
			}
		}
	}

	//If there is no more CTP usng PS_SUBNET
	if (consumer_ptr == 0){
		return 1; //Return 1 meaning that the circuit-switching was completed
	}

	request_SDN_path(source_pe, target_pe);

	//Return 1 meaning that the circuit-switching was NOT completed yet
	return 0;
}

int is_CS_not_active(){
	return (consumer_ptr == 0);
}


void handle_connection_response(unsigned int source_pe, unsigned int target_pe, int subnet){

	int cs_complete;

	while (producer_task_ptr->allocated_proc != source_pe){
		Puts("ERROR: source PE does not match\n");
	}
	while(get_task_location(consumer_ptr->id) != target_pe){
		Puts("ERROR: consumer PE does not match\n");
	}

	Puts("Subnet: "); Puts(itoa(subnet)); Puts(" defined\n");

	consumer_ptr->subnet = subnet;

	//Request the next pending connection for the application
	cs_complete = request_connection(app_ctp_ptr);

	//When the app have no more CTP to establishes a connection set it as READY_TO_LOAD
	if (cs_complete){
		Puts("App READY_TO_LOAD\n");
		app_ctp_ptr->status = READY_TO_LOAD;
	}

}

void handle_SDN_ack(unsigned int * recv_message){

	unsigned int * message;
	static unsigned int global_path_counter = 0;
	int is_global = 0, local_success_rate = 0, global_success_rate = 0;
	unsigned int source, target, subnet, aux_address, connection_ok, sx, sy, tx, ty;
	int path_size, overhead;

	overhead = GetTick();
	Puts("ACK received from SDN controller at time: "); Puts(itoa(overhead)); Puts("\n");

	connection_ok	= recv_message[2];
	source 			= recv_message[3];
	target 			= recv_message[4];
	subnet			= recv_message[5];
	overhead 		= recv_message[6]; //Enabling to compute the pure path overhead
	//overhead 		= cpu_time_used; //Gets the path overhead at the requester perspective
	is_global 		= recv_message[7];
	path_size 		= recv_message[8];


	Puts("PATH NR: "); Puts(itoa(++global_path_counter));
	Puts(" - ACK received from ["); Puts(itoa(recv_message[1]));
	Puts("] sucess ["); Puts(itoa(connection_ok));
	Puts("] source [");  Puts(itoa(source >> 8)); Puts("x"); Puts(itoa(source & 0xFF));
	Puts("] target ["); Puts(itoa(target >> 8)); Puts("x"); Puts(itoa(target & 0xFF));
	Puts("] subnet ["); Puts(itoa(subnet)); Puts("]\n\n");

	handle_connection_response(source, target, subnet);

}


/** Each time that this function is called it send a packet SET_INITIAL_CS_PRODUCER. This causes one round of the
 * protocol to setup CS at the beggining of a secure app.
 *
 */
int initial_CS_setup_protocol(Application * app_ptr, int prod_task, int cons_task){
	Puts("AQUUII\n\n");

	int prod_address, cons_address;
	unsigned int * send_message;
	ConsumerTask * ct;
	Task * t;

	//I receives the prod_task to the for loop continue in the same task than previously
	for(int i = prod_task; i<app_ptr->tasks_number; i++){
		t = &app_ptr->tasks[i];

		//K receives (cons_task+1) to the for loop considerar the next task
		for(int k = (cons_task+1); k < t->consumers_number; k++){
			ct = &t->consumer[k];

			if (ct->id != -1 && ct->subnet != PS_SUBNET){

				prod_task = t->id;
				cons_task = ct->id;

				send_message = get_message_slot();

				prod_address = get_task_location(prod_task);
				cons_address = get_task_location(cons_task);

				//Message is always sent to producer task
				send_message[0] = cons_address;
				send_message[1] = CONSTANT_PKT_SIZE;
				send_message[2] = cons_task;
				send_message[3] = get_task_location(cons_task);


			}

		}

		return 1; //Return 1 se achou
	}


	return 0;

}


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


#endif /* APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_ */
