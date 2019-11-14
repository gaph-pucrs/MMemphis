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


#endif /* APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_ */
