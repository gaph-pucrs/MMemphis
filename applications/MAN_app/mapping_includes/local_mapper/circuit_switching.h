/*
 * circuit_switching.h
 *
 *  Created on: 8 de nov de 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_
#define APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_

#include "application.h"

//CS Utilization Matrix status
#define CS_UTIL_OUTDATED 	0
#define CS_UTIL_REQUESTED	1
#define CS_UTIL_UPDATED		2

/*Stores the utilization of the local in (0xF0) and local out (0x0F) port for each router*/
unsigned char cs_utilization[MAPPING_XCLUSTER][MAPPING_YCLUSTER];
unsigned char cs_utilization_updated = CS_UTIL_OUTDATED;
Application   * app_ctp_ptr = 0;
Task		  * producer_task_ptr = 0;
ConsumerTask  * consumer_ptr = 0;

void request_connection(Application * app){
 //1. Organize the CTPs
 //2. Send they to the SDN controller
 //3. Set app waiting reclustering
	ConsumerTask * ct;
	Task * t;
	unsigned int source_pe, target_pe;
	int task_index, ct_index;

	//Puts("\n---- Request connection\n");
	target_pe = 0;
	task_index=0;
	ct_index=0;

	//ARRUMAR
	if (app_ctp_ptr){
		//Sets task_index in the last task index
		for(; task_index<app->tasks_number; task_index++){
			t = &app->tasks[task_index];
			if (t == producer_task_ptr)
				break;
		}

		//Sets the ct_index in the last consumer task index
		for(; ct_index < t->consumers_number; ct_index++){
			ct = &t->consumer[ct_index];
			if (ct == consumer_ptr){
				ct_index++; //Increments to the algorithm below get the next ctp
				break;
			}
		}
	}


	for(; task_index<app->tasks_number; task_index++){
		t = &app->tasks[task_index];

		source_pe = t->allocated_proc;

		for(; ct_index<t->consumers_number; ct_index++){
			ct = &t->consumer[ct_index];

			if (ct->id != -1 && ct->subnet == PS_SUBNET){
				target_pe = get_task_location(ct->id);

				if (source_pe == target_pe){
					//Puts("CS not necessary\n");
					continue;
				}

				Puts("Requesting CS for ctp: "); Puts(itoa(t->id)); putsv(" -> ", ct->id);

				app_ctp_ptr = app;
				producer_task_ptr = t;
				consumer_ptr = ct;


				request_SDN_path(source_pe, target_pe, -1);
				//After request CS it is safe to set the cs utilization as not updated
				cs_utilization_updated = CS_UTIL_OUTDATED;

				//Return 0 meaning that the circuit-switching was NOT completed yet
				return;
			}
		}

		ct_index = 0;
	}

	Puts("SDN complete - app READY_TO_LOAD\n\n");
	app->status = READY_TO_LOAD;

	//If there is no more CTP usng PS_SUBNET
	app_ctp_ptr = 0;
	producer_task_ptr = 0;
	consumer_ptr = 0;
}

int is_CS_not_active(){
	return (consumer_ptr == 0);
}


void handle_connection_response(unsigned int source_pe, unsigned int target_pe, int subnet){

	while (producer_task_ptr->allocated_proc != source_pe){
		Puts("ERROR: source PE does not match\n");
	}
	while(get_task_location(consumer_ptr->id) != target_pe){
		Puts("ERROR: consumer PE does not match\n");
	}

	if (subnet > -1) {
		consumer_ptr->subnet = subnet;
		//Puts("Subnet: "); Puts(itoa(subnet)); Puts(" defined\n\n");
	} else {
		consumer_ptr->subnet = PS_SUBNET;
		//Puts("Subnet PS_SUBNET defined\n\n");
	}

	//Request the next pending connection for the application
	request_connection(app_ctp_ptr);

}

void handle_SDN_ack(unsigned int * recv_message){

	//static unsigned int global_path_counter = 0;
	int is_global = 0;
	unsigned int source, target, subnet, connection_ok;
	int path_size, overhead;

	overhead = GetTick();
	//Puts("ACK received from SDN controller at time: "); Puts(itoa(overhead)); Puts("\n");

	connection_ok	= recv_message[2];
	source 			= recv_message[3];
	target 			= recv_message[4];
	subnet			= recv_message[5];
	overhead 		= recv_message[6]; //Enabling to compute the pure path overhead
	//overhead 		= cpu_time_used; //Gets the path overhead at the requester perspective
	is_global 		= recv_message[7];
	path_size 		= recv_message[8];


	//Puts("PATH NR: "); Puts(itoa(++global_path_counter));
	Puts(" - ACK received from ["); Puts(itoa(recv_message[1]));
	Puts("] sucess ["); Puts(itoa(connection_ok));
	Puts("] source [");  Puts(itoa(source >> 8)); Puts("x"); Puts(itoa(source & 0xFF));
	Puts("] target ["); Puts(itoa(target >> 8)); Puts("x"); Puts(itoa(target & 0xFF));
	Puts("] subnet ["); Puts(itoa(subnet)); Puts("]\n");

	handle_connection_response(source, target, subnet);

}


/** Each time that this function is called it send a packet SET_INITIAL_CS_PRODUCER. This causes one round of the
 * protocol to setup CS at the beggining of a secure app.
 *
 */
int initial_CS_setup_protocol(Application * app_ptr, int prod_task, int cons_task){
	int prod_address, cons_address, prod_index, cons_index;
	unsigned int * send_message;
	ConsumerTask * ct = 0;
	Task * t = 0;

	//Alinha prod_index e cons_index com a ultima execucao da funcao, poderia usar static mas prefiro nao usar data memory region

	if (prod_task){
		prod_index = -1;
		cons_index = -1;
		do{
			prod_index++; //MAIS MAIS
			t = &app_ptr->tasks[prod_index];
		} while(t->id != prod_task);

		do {
			cons_index++;
			ct = &t->consumer[cons_index];
		} while(ct->id != cons_task);
		cons_index++;
	} else{
		Puts("\nInit CS protocol...\n");
		prod_index = 0;
		cons_index = 0;
	}


	//I receives the prod_task to the for loop continue in the same task than previously
	for(; prod_index<app_ptr->tasks_number; prod_index++){
		t = &app_ptr->tasks[prod_index];

		for(; cons_index < t->consumers_number; cons_index++){

			ct = &t->consumer[cons_index];

			//Message sent to the consumer task
			if (ct->id != -1 && ct->subnet != PS_SUBNET){

				prod_task = t->id;
				cons_task = ct->id;

				prod_address = get_task_location(prod_task);
				cons_address = get_task_location(cons_task);

				Puts("Setting up to "); Puts(itoa(prod_task)); putsv(" -> ", cons_task);

				//Usefull info: prod_ID, prod_PE, cons_ID, cons_PE, master id, master addr
				//Message is always sent to producer task
				send_message = get_message_slot();
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

		}

		cons_index = 0;

	}

	//There is no more CTP to setup CS at kernel, terminates the CS protocol
	Puts("Initial CS protocol concluded!\n\n");
	return 0;

}


void request_cs_util_update(){
	int SDN_Controller_ID = 2; //TODO Please edit when you add a new MA task
	unsigned int * send_message;

	Puts("\nSend CS_UTILIZATION_REQUEST\n");

	cs_utilization_updated = CS_UTIL_REQUESTED;

	send_message = get_message_slot();
	send_message[0] = CS_UTILIZATION_REQUEST;
	send_message[1] = GetMyID(); //source
	send_message[2] = cluster_x_offset << 8 | cluster_y_offset; //target
	send_message[3] = MAPPING_XCLUSTER << 8 | MAPPING_YCLUSTER;

	SendService(SDN_Controller_ID, send_message, 4);

}

void handle_cs_utilization_response(unsigned int * data_msg){
	int index;//, conn_in, conn_out;

	Puts("CS_UTILIZATION_RESPONSE received\n\n");

	cs_utilization_updated = CS_UTIL_UPDATED;

	index = 1;

	for(int y = 0; y < MAPPING_YCLUSTER; y++){
		for (int x = 0; x < MAPPING_XCLUSTER; x++){

			//conn_in = data_msg[index] >> 16;
			//conn_out = data_msg[index] & 0xFFFF;

			//Puts(itoa(x)); Puts("x"); Puts(itoa(y)); Puts(": in:");
			//Puts(itoa(conn_in)); putsv(", out:", conn_out);

			//cs_utilization[x][y] = (unsigned char) ((conn_in << 4) | conn_out);

			cs_utilization[x][y] = data_msg[index];

			//Puts(itoa(x)); Puts("x"); Puts(itoa(y));
			//putsv(", free:", data_msg[index]);

			index++;
		}
	}

}

unsigned int compute_bounding_box_cs_utilization(int x_min, int y_min, int x_max, int y_max){

	unsigned int accumulated_util = 0;
	unsigned int conv_utilzation;

	//Puts("Computing BB utilization\n");

	for(int x=x_min; x<=x_max; x++){
		for(int y=y_min; y<=y_max; y++){
			conv_utilzation = (unsigned int) cs_utilization[x][y];
			//Puts(itoa(x)); Puts("x"); Puts(itoa(y));
			//putsv(", free:",conv_utilzation);
			accumulated_util = accumulated_util + conv_utilzation;
		}
	}

	return accumulated_util;
}


#endif /* APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_ */
