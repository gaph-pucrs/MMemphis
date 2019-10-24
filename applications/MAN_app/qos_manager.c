/*
 * qos_manager.c
 *
 *  Created on: 11 de out de 2019
 *      Author: ruaro
 */
#include "common_include.h"

void initialize_qos_manager(unsigned int * msg){
	unsigned int max_ma_tasks, task_id, proc_addr, id_offset, x_aux, y_aux;

	task_id = msg[1];
	id_offset = msg[2];

	Puts("\n ************ Initialize QoS Manager *********** \n");
	Puts("Task ID: "); Puts(itoa(task_id)); Puts("\n");
	Puts("Offset ID: "); Puts(itoa(id_offset)); Puts("\n");
	Puts("MAX_MA_TASKS: "); Puts(itoa(msg[3])); Puts("\n");

	max_ma_tasks = msg[3];

	//Set the proper location of all MA tasks
	for(int i=0; i<max_ma_tasks; i++){
		task_id = msg[i+4] >> 16;
		proc_addr = msg[i+4] & 0xFFFF;
		Puts("Task MA "); Puts(itoa(task_id)); Puts(" allocated at "); Puts(itoh(proc_addr)); Puts("\n");
		AddTaskLocation(task_id, proc_addr);
	}

	init_generic_send_comm(id_offset, MAPPING_XCLUSTER);

	Puts("QoS Manager initialized!!!!!!\n\n");
}


void fires_SDN_request(){

	while(GetTick() < 200000);

	request_SDN_path(0x200, 0x102);
}

void handle_SDN_ack(unsigned int * recv_message){

	unsigned int * message;
	static unsigned int global_path_counter = 0;
	int is_global = 0, local_success_rate = 0, global_success_rate = 0;
	unsigned int source, target, subnet, aux_address, connection_ok, sx, sy, tx, ty;
	int path_size, overhead;

	//Puts("ACK received from SDN controller at time: "); Puts(itoa(GetTick())); Puts("\n");

	connection_ok	= recv_message[2];
	source 			= recv_message[3];
	target 			= recv_message[4];
	subnet			= recv_message[5];
	overhead 		= recv_message[6]; //Enabling to compute the pure path overhead
	//overhead 		= cpu_time_used; //Gets the path overhead at the requester perspective
	is_global 		= recv_message[7];
	path_size 		= recv_message[8];


	Puts("\nPATH NR: "); Puts(itoa(++global_path_counter));
	Puts(" - ACK received from ["); Puts(itoa(recv_message[1]));
	Puts("] sucess ["); Puts(itoa(connection_ok));
	Puts("] source [");  Puts(itoa(source >> 8)); Puts("x"); Puts(itoa(source & 0xFF));
	Puts("] target ["); Puts(itoa(target >> 8)); Puts("x"); Puts(itoa(target & 0xFF));
	Puts("] subnet ["); Puts(itoa(subnet)); Puts("]\n\n");


	message = get_message_slot();
	message[0] = target;
	message[1] = 11; //payload
	message[2] = SET_NOC_SWITCHING_CONSUMER;
	message[3] = 257;//p->producer_task
	message[4] = 256;//p->consumer_task
	message[5] = GetNetAddress(); //Source PE - QoS master addr
	message[9] = GetMyID(); //task_number- QoS master ID
	message[10] = subnet;//p->cs_net
	message[11] = 1;//p->cs_mode 1 establish. 0 release
	SendRaw(message, 13);

	Puts("SET_NOC_SWITCHING_CONSUMER CS mode sent\n");
}

void CTP_set_PS_switching(unsigned int consumer_task, unsigned int producer_task, unsigned int consumer_addr){

	unsigned int * message;

	message = get_message_slot();
	message[0] = consumer_addr;
	message[1] = 11; //payload
	message[2] = SET_NOC_SWITCHING_CONSUMER;
	message[3] = producer_task;//p->producer_task
	message[4] = consumer_task;//p->consumer_task
	message[5] = GetNetAddress(); //Source PE - QoS master addr --used
	message[9] = GetMyID(); //task_number- QoS master ID --used
	//message[10] = subnet;//p->cs_net -- there is no more subnet since the path is PS
	message[11] = 0;//p->cs_mode 1 establish. 0 release CS and use PS
	SendRaw(message, 13);

	Puts("SET_NOC_SWITCHING_CONSUMER PS mode sent\n");

}

void handle_message(unsigned int * data_msg){
	static unsigned int flag_apagar = 1;

	switch (data_msg[0]) {
		case INITIALIZE_MA_TASK:
			initialize_qos_manager(data_msg);
			/*fires_SDN_request();*/
			break;
		case PATH_CONNECTION_ACK:
			handle_SDN_ack(data_msg);
			break;
		case NOC_SWITCHING_CTP_CONCLUDED:
			Puts(" ACK received, CS protocol finished\n");
			/*while(flag_apagar){
				while(GetTick() > 400000 ){
					Puts("aqui\n");
					flag_apagar = 0;
					CTP_set_PS_switching(256, 257, 0x102);
					break;
				}
			}*/
			break;
		default:
			Puts("Error message unknown\n");
			for(;;);
			break;
	}

}


void main(){

	RequestServiceMode();
	init_message_slots();
	initialize_MA_task();

	unsigned int data_message[MAX_MANAG_MSG_SIZE];

	for(;;){
		ReceiveService(data_message);
		handle_message(data_message);
	}

	exit();
}
