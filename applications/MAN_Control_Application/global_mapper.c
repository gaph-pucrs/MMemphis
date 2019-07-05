/*
 * global_mapper.c
 *
 *  Created on: Jul 1, 2019
 *      Author: ruaro
 */
#include "common_include.h"
#include "global_mapper_modules/cluster_controller.h"

void instantiate_mapping_app(){

	unsigned int cluster_id = 0;
	Cluster * cl_ptr;

	for(int y=0; y<CLUSTER_Y_SIZE; y++){
		for(int x=0; x<CLUSTER_X_SIZE; x++){

			cl_ptr = &clusters[cluster_id];

			/*Initialize Clusters*/
			cl_ptr->x_pos = x;
			cl_ptr->y_pos = y;
			//Aqui o mapeamentos dos lcoal mapper eh definido
			cl_ptr->free_resources = (x*XCLUSTER << 8) | (y*YCLUSTER); //Reusing of this variable only for initialization

			Puts("Cluster id "); Puts(itoa(cluster_id)); Puts("\nLocated at proc: "); Puts(itoh(cl_ptr->free_resources)); Puts("\n");

			if (x == 0 && y == 0)
				//Esse +1 eh pq no PE 0x0 vai ficar o global mapper e o local mapper0x0 deve ficar em um PE diferente, portanto movi +1 pra direita
				cl_ptr->free_resources = ((x*XCLUSTER << 8)+1) | (y*YCLUSTER);

			//Request to the injector to load the local_mapper inside the PEs
			send_task_allocation_message(1, cl_ptr->free_resources, 0);

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
		//Mapping in progress is being reused here, only for initializaiton, after that, the variable stores if a cluster is performing mapping or not
		if (clusters[i].free_resources == source_addr){

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
			message[index++] = clusters[i].free_resources;
			Puts("ID "); Puts(itoa((i+1)));
			Puts("\naddr: "); Puts(itoh(clusters[i].free_resources)); Puts("\n");
		}

		for(int i=0; i<CLUSTER_NUMBER; i++){
			//Add task location temporally
			AddTaskLocation(1, clusters[i].free_resources);
			message[1] = (clusters[i].x_pos << 8 | clusters[i].y_pos);
			SendService(1, message, index);
		}

		Puts("Local Mappers Initialization completed!\n");

		//Update task location definitively
		for(int i=0; i<CLUSTER_NUMBER; i++){
			AddTaskLocation(i+1, clusters[i].free_resources);
			//From this moment this variable <free_resources> starts to store its real meaning
			if (i==0) //If the cluster has ID 0, remove two resources from its cluster
				clusters[i].free_resources = (MAX_LOCAL_TASKS * XCLUSTER * YCLUSTER) - 2;
			else
				clusters[i].free_resources = (MAX_LOCAL_TASKS * XCLUSTER * YCLUSTER) - 1;
		}

		message = get_message_slot();
		message[0] = APP_INJECTOR;
		message[1] = 1;
		message[2] = APP_MAPPING_COMPLETE;
		SendIO(message, 3);
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
		Puts("Cluster full\n");
		return;
	}

	pending_app_req = 0;

	Puts("\n\nNEW_APP_REQ\n");
	Puts(itoa(app_cluster_id));
	Puts("\nTask number: "); Puts(itoa(app_task_number));
	Puts("\n");

	Puts("\nNew app req handled! - cluster mapping is ");

	if (app_cluster_id == CLUSTER_NUMBER){
		Puts("dynamic\n");
		app_cluster_id = search_cluster(app_task_number);
	} else {
		Puts("static\n");
	}

	Puts("-----> NEW_APP_REQ from app injector. Task number is "); Puts(itoa(app_task_number));
	Puts("\nApp mapped at cluster "); Puts(itoh(  clusters[app_cluster_id].x_pos << 8 | clusters[app_cluster_id].y_pos )); Puts("\n");
	Puts("\nApplication ID: "); Puts(itoa(app_id_counter)); Puts("\n");

	//putsv("Global Master reserve application: ", num_app_tasks);
	//putsv("total_mpsoc_resources ", total_mpsoc_resources);

	decrease_avail_mpsoc_resources(app_task_number);

	cluster_loc = (app_cluster_id+1) << 16 | GetTaskLocation(app_cluster_id+1);

	Puts("Cluster loc "); Puts(itoa(cluster_loc)); Puts("\n");

	/*Sends packet to APP INJECTOR requesting repository to cluster app_cluster_id*/
	message = get_message_slot();
	message[0] = APP_INJECTOR;
	message[1] = 3; //Payload size
	message[2] = APP_REQ_ACK;
	message[3] = app_id_counter;
	message[4] = cluster_loc; //PLus 1 because as the global mapper is in ID 0 it shift all other IDs in more 1
	SendIO(message, 5);
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
	init_message_slots();
	instantiate_mapping_app();

	unsigned int data_message[MAX_MAPPING_MSG];

	for(;;){

		ReceiveService(data_message);
		handle_message(data_message);

	}

	exit();
}



