/*
 * circuit_switching.h
 *
 *  Created on: 8 de nov de 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_
#define APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_

#include "application.h"

/*Stores the utilization of the local in (0xF0) and local out (0x0F) port for each router*/
unsigned char cs_utilization[MAPPING_XCLUSTER][MAPPING_YCLUSTER];
unsigned char cs_utilization_updated = 0;
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

	//After request CS it is safe to set the cs utilization as not updated
	cs_utilization_updated = 0;

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

	static unsigned int global_path_counter = 0;
	int is_global = 0;
	unsigned int source, target, subnet, connection_ok;
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
	int prod_address, cons_address, cons_index;
	unsigned int * send_message;
	ConsumerTask * ct;
	Task * t;

	cons_index = cons_task + 1;

	//I receives the prod_task to the for loop continue in the same task than previously
	for(int i = prod_task; i<app_ptr->tasks_number; i++){
		t = &app_ptr->tasks[i];

		//K receives (cons_task+1) to the for loop considerar the next task
		while(cons_index < t->consumers_number){

			cons_task = 0;

			ct = &t->consumer[cons_index];

			//Message sent to the consumer task
			if (ct->id != -1 && ct->subnet != PS_SUBNET){

				prod_task = t->id;
				cons_task = ct->id;

				send_message = get_message_slot();

				prod_address = get_task_location(prod_task);
				cons_address = get_task_location(cons_task);


				//Usefull info: prod_ID, prod_PE, cons_ID, cons_PE, master id, master addr
				//Message is always sent to producer task
				send_message[0] = cons_address;				//header
				send_message[1] = CONSTANT_PKT_SIZE-2;		//payload_size
				send_message[2] = SET_INITIAL_CS_CONSUMER;  //service
				send_message[3] = prod_task;				//producer_task
				send_message[4] = cons_task;				//consumer_task
				send_message[8] = prod_address;				//producer_processor
				send_message[9] = cons_address;				//consumer_processor
				send_message[10] = ct->subnet;				//cs_net

				//Create a syscall in case a RT task is running at same proc of local mapper
				/*if (t->allocated_proc == net_address){
					SetInitialCS(send_message, CONSTANT_PKT_SIZE);
				} else {*/
					SendRaw(send_message, CONSTANT_PKT_SIZE);
				//}

				//puts("Sent SET_INITIAL_CS_CONSUMER\n");

				return 1; //Return 1 se achou
			}

			cons_index++;
		}

		cons_index = 0;

	}

	return 0;

}


void request_cs_utilization(){
	int SDN_Controller_ID = 2; //TODO Please edit when you add a new MA task
	unsigned int * send_message;

	Puts("\nSend CS_UTILIZATION_REQUEST\n");

	send_message = get_message_slot();
	send_message[0] = CS_UTILIZATION_REQUEST;
	send_message[1] = GetMyID(); //source
	send_message[2] = cluster_x_offset << 8 | cluster_y_offset; //target
	send_message[3] = MAPPING_XCLUSTER << 8 | MAPPING_YCLUSTER;

	SendService(SDN_Controller_ID, send_message, 4);

}

void handle_cs_utilization_response(unsigned int * data_msg){
	int index, conn_in, conn_out;

	//Puts("\nCS_UTILIZATION_RESPONSE received\n");

	cs_utilization_updated = 1;

	index = 1;

	for(int y = 0; y < MAPPING_YCLUSTER; y++){
		for (int x = 0; x < MAPPING_XCLUSTER; x++){

			conn_in = data_msg[index] >> 16;
			conn_out = data_msg[index] & 0xFFFF;

			Puts(itoa(x)); Puts("x"); Puts(itoa(y)); Puts(": in:");
			Puts(itoa(conn_in)); putsv(", out:", conn_out);

			cs_utilization[x][y] = (unsigned char) ((conn_in << 4) | conn_out);

			index++;
		}
	}

}


#endif /* APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_ */
