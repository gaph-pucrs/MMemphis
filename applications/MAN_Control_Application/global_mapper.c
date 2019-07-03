/*
 * global_mapper.c
 *
 *  Created on: Jul 1, 2019
 *      Author: ruaro
 */
#include "global_mapper.h"
#include "communication.h"
#include <api.h>
#include "common_mapping.h"

/*Common Global Variables*/
Cluster clusters[CLUSTER_NUMBER];
unsigned int total_mpsoc_resources = (MAX_LOCAL_TASKS * XDIMENSION * YDIMENSION);

/*Total MPSoC resources controll*/
int decrease_avail_mpsoc_resources(unsigned int num_tasks){
	if ((total_mpsoc_resources - num_tasks) < 0)
		return 0;

	total_mpsoc_resources -= num_tasks;
	return 1;
}

void increase_avail_mpsoc_resources(int num_tasks){
	total_mpsoc_resources += num_tasks;

	while (total_mpsoc_resources > (MAX_LOCAL_TASKS * XDIMENSION * YDIMENSION))
		puts("ERROR: total_mpsoc_resources higher than MAX\n");
}


void initialize(){

	unsigned int cluster_id = 0;
	for(int y=0; y<CLUSTER_Y_SIZE; y++){
		for(int x=0; x<CLUSTER_X_SIZE; x++){

			/*Initialize Clusters*/
			clusters[cluster_id].x_addr = x;
			clusters[cluster_id].y_addr = y;
			//Aqui o mapeamentos dos lcoal mapper eh definido
			clusters[cluster_id].manager_addr = (x*XCLUSTER << 8) | (y*YCLUSTER);

			Puts("Cluster id "); Puts(itoa(cluster_id)); Puts("\nLocated at proc: "); Puts(itoh(clusters[cluster_id].manager_addr)); Puts("\n");

			if (x == 0 && y == 0)
				//Esse +1 eh pq no PE 0x0 vai ficar o global mapper e o local mapper0x0 deve ficar em um PE diferente, portanto movi +1 pra direita
				clusters[cluster_id].manager_addr = ((x*XCLUSTER << 8)+1) | (y*YCLUSTER);

			//Request to the injector to load the local_mapper inside the PEs
			send_task_allocation_message(1, clusters[cluster_id].manager_addr);

			cluster_id++;
		}
	}

	decrease_avail_mpsoc_resources((cluster_id + 1));

	//Adiciona a si mesmo na tabela da task location
	AddTaskLocation(0, 0);

	//Verificar dimensoes do cluster
	//Requisitar app locais informando o seu ID no INIT
}


void handle_i_am_alive(unsigned int source_addr){
	static unsigned int received_i_am_alive_counter = 0;
	char error_flag = 1;
	unsigned int * message;
	unsigned int index = 0;

	Puts("handle_i_am_alive from "); Puts(itoa(source_addr)); Puts("\n");

	for(int i=0; i < CLUSTER_NUMBER; i++ ){
		if (clusters[i].manager_addr == source_addr){

			//Adiciona o ID do cluster ao seu endereco verdadeiro
			//AddTaskLocation((i+1), source_addr);

			received_i_am_alive_counter++;

			error_flag = 0;
			break;
		}
	}

	Puts("I am alive: "); Puts(itoa(received_i_am_alive_counter)); Puts("\n");

	while (error_flag)
		Puts("ERROR: received address does not belongs to any cluster\n");

	if (received_i_am_alive_counter == CLUSTER_NUMBER){

		Puts("Initilizing local mappers\n");

		message = get_message_slot();
		message[0] = INIT_LOCAL;
		message[1] = 0; //Empty, will be filled for each thread
		message[2] = 0; //ID do global mapper
		message[3] = 0; //Address do global mapper
		index = 4;
		for(int i=0; i<CLUSTER_NUMBER; i++){
			message[index++] = (i+1); //Cluster task ID, +1 pq o global mapper possui o ID 0
			message[index++] = clusters[i].manager_addr;
			Puts("ID "); Puts(itoa((i+1)));
			Puts("\naddr: "); Puts(itoh(clusters[i].manager_addr)); Puts("\n");
		}

		for(int i=0; i<CLUSTER_NUMBER; i++){
			//Add task location temporally
			AddTaskLocation(1, clusters[i].manager_addr);
			message[1] = (clusters[i].x_addr << 8 | clusters[i].y_addr);
			SendService(1, message, index);
		}

		//Update task location definitively
		for(int i=0; i<CLUSTER_NUMBER; i++)
			AddTaskLocation(i+1, clusters[i].manager_addr);
	}

}

void handle_new_app_req(unsigned int app_cluster_id, unsigned int app_task_number){
	Puts("\n\nNEW_APP_REQ\n");
	Puts(itoa(app_cluster_id));
	Puts("\nTask number: "); Puts(itoa(app_task_number));
	Puts("\n");

	/*If there is enought system resources*/
	if (decrease_avail_mpsoc_resources(app_task_number)){

	}


}

void handle_message(unsigned int * data_msg){

	switch (data_msg[0]) {
		case INIT_I_AM_ALIVE:
			handle_i_am_alive(data_msg[1]);
			break;
		case NEW_APP_REQ:
			handle_new_app_req(data_msg[1], data_msg[2]);
			break;
		default:
			Puts("Error message unknown\n");
			for(;;);
			break;
	}
}

void main(){

	RequestServiceMode();

	Echo("Initializing Global Mapper\n");
	init_communication();
	initialize();

	unsigned int data_message[MAX_MAPPING_MSG];

	for(;;){

		ReceiveService(data_message);
		handle_message(data_message);

	}

	exit();
}



