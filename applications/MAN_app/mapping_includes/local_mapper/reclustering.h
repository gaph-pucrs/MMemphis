/*
 * reclustering.h
 *
 *  Created on: Jul 4, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_RECLUSTERING_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_RECLUSTERING_H_

#include "application.h"
#include "resoucer_controller.h"
#include "globals.h"

#define RECLUSTERING_DEBUG	0		//!<When enable compile the puts in this file

typedef struct {
	Task * task;				//!<Task pointer of the task under reclustering
	int active;					//!<Reclustering status (active 1, or disabled 0)
	int pending_loan_delivery;	//!<Number of pending load delivery which the master is waiting
	int min_loan_proc_hops;		//!<Minimum number of hops from borrowed processor
	int current_borrowed_proc;	//!<Borrowed processor address
	int current_borrowed_master;//!<Borrowed processor master address
	int neighbors_level;		//!<Number of neighbors levels (clusters levels) used in the reclustering protocol
} Reclustering;


int is_reclustering_NOT_active();

void init_reclustering();

void handle_reclustering(unsigned int *);

int reclustering_next_task(Application *);


Reclustering reclustering;			//!<Reclustering structure instance

//Max o levels of MPSoCs
int max_neighbors_level = 0;		//!<Max of neighbors clusters for each reclustering level

//Cluster dimmensions
unsigned int starting_x_cluster_size;	//!<Starting X size of cluster when reclustering is started
unsigned int starting_y_cluster_size;	//!<Starting Y size of cluster when reclustering is started

/**Setup a reclustering, called by kernel and configure the cluster ID and initializes some variables
 * \param c_id Current cluster ID
 */
void init_reclustering(){

	starting_x_cluster_size = MAPPING_XCLUSTER;
	starting_y_cluster_size = MAPPING_YCLUSTER;

#if RECLUSTERING_DEBUG
	Puts("starting_x_cluster_size: "); Puts(itoa(starting_x_cluster_size)); Puts("\n");
	Puts("starting_y_cluster_size: "); Puts(itoa(starting_y_cluster_size)); Puts("\n");
#endif

	reclustering.active = 0;

	if ((XDIMENSION/MAPPING_XCLUSTER) > (YDIMENSION/MAPPING_YCLUSTER)) {
		max_neighbors_level = (XDIMENSION/MAPPING_XCLUSTER) - 1;
	} else {
		max_neighbors_level = (YDIMENSION/MAPPING_YCLUSTER) - 1;
	}
}

/**Test if the reclustering is not active
 * \return 1 if is not active, 0 if is active
 */
int is_reclustering_NOT_active(){
	return !reclustering.active;
}

/**Assembles and sends a LOAN_PROCESSOR_REQUEST packet to the neighbor master kernel
 * \param address Neighbor master address
 * \param taskID Task ID target of reclustering
 */
void send_loan_proc_request(int target_cluster, int taskID){

	int initial_proc;
	Application * app_ptr = get_application_ptr(taskID >> 8);

	if (app_ptr->tasks[0].allocated_proc == -1){
		initial_proc = net_address;
	} else {
		initial_proc = app_ptr->tasks[0].allocated_proc;
	}

	unsigned int * message = get_message_slot();
	message[0] = LOAN_PROCESSOR_REQUEST;
	message[1] = cluster_position; //Uses the manager PE as reference
	message[2] = taskID; //task id
	message[3] = initial_proc; //Allocated proc reference

	send(target_cluster, message, 4);

#if RECLUSTERING_DEBUG
	Puts("-> send loan proc REQUEST para proc "); Puts(itoh(target_cluster)); Puts(" task id "); Puts(itoa(taskID)); Puts(" initial proc "); Puts(itoh(message[3])); Puts("\n");
#endif

}

/**Assembles and sends a LOAN_PROCESSOR_DELIVERY packet to the target of reclustering (requesting) master kernel
 * \param master_address Requesting reclustering master address
 * \param taskID Task ID target of reclustering
 * \param proc Allocated processor
 * \param hops Number of hops from the target processor
 */
void send_loan_proc_delivery(int target_position, int taskID, int proc, int hops){

	unsigned int * message = get_message_slot();
	message[0] = LOAN_PROCESSOR_DELIVERY;
	message[1] = cluster_position;
	message[2] = taskID;
	message[3] = hops;
	message[4] = proc;

	send(target_position, message, 5);

#if RECLUSTERING_DEBUG
	Puts("-> send loan proc DELIVERY para proc "); Puts(itoh(target_position)); putsv(" task id ", taskID);
#endif

}

/**Assembles and sends a LOAN_PROCESSOR_RELEASE packet to the neighbor master kernel
 * \param master_address Neighbor master address
 * \param release_proc Address of the released processor
 * \param taskID Task ID target of reclustering
 */
void send_loan_proc_release(int target_position, int release_proc, int taskID){

	unsigned int * message = get_message_slot();
	message[0] = LOAN_PROCESSOR_RELEASE;
	message[1] = cluster_position;
	message[2] = release_proc;
	message[3] = taskID;

	send(target_position, message, 4);

#if RECLUSTERING_DEBUG
	Puts("-> send loan proc RELEASE para proc "); Puts(itoh(target_position)); Puts(" releasing the proc "); Puts(itoh(release_proc)); Puts("\n");
#endif

}

/**Fires a new round of the reclustering protocol. This function send the load request to the neighboors masters
 * delimited by the reclustering.neighbors_level variable
 */
void fires_reclustering_protocol(){

	int other_x, other_y;
	int this_x, this_y;
	int hops_x, hops_y;
	int cluster_x_size, initial_x_level;
	int cluster_y_size, initial_y_level;
	int other_address;

#if RECLUSTERING_DEBUG
	putsv(" Fires reclustering protocol - level ", reclustering.neighbors_level);
#endif

	reclustering.pending_loan_delivery = 0;
	reclustering.current_borrowed_proc = -1;
	reclustering.current_borrowed_master = -1;
	reclustering.min_loan_proc_hops = 0xFFFFFF;

	cluster_x_size = starting_x_cluster_size; //+ 1 para calcular o numero de processadores do eixo x e nao o numero de hops
	cluster_y_size = starting_y_cluster_size;

#if RECLUSTERING_DEBUG
	//putsv("starting_x_cluster_size: ", starting_x_cluster_size);
	//putsv("starting_y_cluster_size: ", starting_y_cluster_size);
#endif

	initial_x_level = cluster_x_size * (reclustering.neighbors_level -1);
	initial_y_level = cluster_y_size * (reclustering.neighbors_level -1);

	cluster_x_size *= reclustering.neighbors_level;
	cluster_y_size *= reclustering.neighbors_level;

	this_x = cluster_x_offset;
	this_y = cluster_y_offset;

#if RECLUSTERING_DEBUG
	//putsv("this_x: ", this_x);
	//putsv("this_y: ", this_y);
#endif

	//Walks for every cluster inside the level
	for(int y=0; y<MAPPING_Y_CLUSTER_NUM; y++){
		for(int x=0; x<MAPPING_X_CLUSTER_NUM; x++){

			other_x = x * MAPPING_XCLUSTER;
			other_y = y * MAPPING_YCLUSTER;

#if RECLUSTERING_DEBUG
			//putsv("current_position: ", (x << 8 | y));
#endif

			if (cluster_position == (x << 8 | y)){
				continue;
			}

#if RECLUSTERING_DEBUG
			//putsv("initial_x_level: ", initial_x_level);
			//putsv("initial_y_level: ", initial_y_level);

			//putsv("other_x: ", other_x);
			//putsv("other_y: ", other_y);
#endif


			hops_x = abs(this_x - other_x);
			hops_y = abs(this_y - other_y);

			if ( (hops_x > initial_x_level || hops_y > initial_y_level) &&
					hops_x <= cluster_x_size && hops_y <= cluster_y_size) {

				other_address = (x << 8 | y);

				send_loan_proc_request(other_address, reclustering.task->id);

				reclustering.pending_loan_delivery++;
			}

		}

	}

}

/**handle reclustering packets incoming from kernel master.
 * The kernel master kwnows that the packet is a reclustering related packet and
 * calls this function passing the ServiceHeader pointer.
 * \param p ServiceHeader pointer
 */
void handle_reclustering(unsigned int * msg){

	int mapped_proc;
	Application *app;
	int hops, ref_x, ref_y, curr_x, curr_y;

	switch(msg[0]){

	case LOAN_PROCESSOR_REQUEST:

		/*
			msg[1] = cluster_position; //Uses the manager PE as reference
			msg[2] = taskID; //task id
			msg[3] = ref proc; //Allocated proc reference
		 */

#if RECLUSTERING_DEBUG
		Puts("\nReceive LOAN_PROCESSOR_REQUEST "); Puts(" from master "); Puts(itoh(msg[1])); Puts(" ref proc "); Puts(itoh(msg[3])); putsv(" task id ", msg[2]);
#endif

		if (free_resources < 1){

			send_loan_proc_delivery(msg[1], msg[2], -1, -1);

		} else {

			//Procura pelo processador mais proximo do processador requisitnate

			mapped_proc = reclustering_map(msg[3]);


			//Puts("\nRef proc for task "); Puts(itoa(msg[2])); Puts(" is: "); Puts(itoh(msg[3])); Puts("\n");

			ref_x = (msg[3] >> 8);
			ref_y = (msg[3] & 0xFF);
			curr_x = (mapped_proc >> 8);
			curr_y = (mapped_proc & 0xFF);

			hops = (abs(ref_x - curr_x) + abs(ref_y - curr_y));

#if RECLUSTERING_DEBUG
			Puts("Alocou proc "); Puts(itoh(mapped_proc)); putsv(" hops ", hops); Puts("\n");
#endif

			send_loan_proc_delivery(msg[1], msg[2], mapped_proc, hops);

			page_used(mapped_proc, msg[2]);

		}

		break;

	case LOAN_PROCESSOR_DELIVERY:

		/*
		msg[1] = cluster_position;
		msg[2] = taskID;
		msg[3] = hops; //p->hops
		msg[4] = proc; p->allocated_processor = proc;
		*/

		reclustering.pending_loan_delivery--;

#if RECLUSTERING_DEBUG
		Puts("\nReceive LOAN_PROCESSOR_DELIVERY "); Puts(" from proc "); Puts(itoh(msg[1] ));  Puts(" hops "); Puts(itoa(msg[3])); Puts("\n");
		putsv("Numeros de delivery pendendtes: ", reclustering.pending_loan_delivery);
#endif

		if (msg[4] != -1){

			if (msg[3] < reclustering.min_loan_proc_hops){

#if RECLUSTERING_DEBUG
				Puts("Alocou processador "); Puts(itoh(msg[4])); Puts("\n");
#endif

				reclustering.min_loan_proc_hops = msg[3];

				if (reclustering.current_borrowed_proc != -1){

					send_loan_proc_release(reclustering.current_borrowed_master, reclustering.current_borrowed_proc, msg[2]);
				}

				reclustering.current_borrowed_proc = msg[4];

				reclustering.current_borrowed_master = msg[1];

			} else {

				send_loan_proc_release(msg[1], msg[4], msg[2]);

			}
		}

		if (reclustering.pending_loan_delivery == 0){

#if RECLUSTERING_DEBUG
			Puts("Fim da rodada\n");
#endif

			//handle end of reclustering round()
			if (reclustering.current_borrowed_proc != -1){

				//At this point the reclustering has benn finished successifully
				Puts("Reclustering acabou com sucesso, tarefa alocada no proc "); Puts(itoh(reclustering.current_borrowed_proc)); putsv(" com id ", reclustering.task->id);

				reclustering.active = 0;

				reclustering.task->allocated_proc = reclustering.current_borrowed_proc;

				reclustering.task->borrowed_master = reclustering.current_borrowed_master;

				//Searches for a new task waiting reclustering
				app = get_application_ptr((reclustering.task->id >> 8));

				reclustering_next_task(app);

				break;



			} else {

				reclustering.neighbors_level++;

				if (reclustering.neighbors_level > max_neighbors_level){

					reclustering.neighbors_level = 1;
#if RECLUSTERING_DEBUG
					Puts("Warning: reclustering repeated!\n");
#endif

				}

				fires_reclustering_protocol();
			}
		}

		break;

	case LOAN_PROCESSOR_RELEASE:

		/*
			msg[1] = cluster_position;
			msg[2] = release_proc;
			msg[3] = taskID;
		 */
#if RECLUSTERING_DEBUG
		Puts("\nReceive LOAN_PROCESSOR_RELEASE "); Puts(" from proc "); Puts(itoh(msg[1])); putsv(" task id ", msg[2]);
#endif

		page_released(msg[2], msg[3]);

		break;
	}
}

/**Searches for first application task not mapped yet, firing the reclustering
 * \param app Application pointer
 * \return 1 - if the application have all its task mapped, 0 - if the reclustering was initiated
 */
int reclustering_next_task(Application *app){

	Task *t = 0;

	/*Select the app task waiting for reclustering*/
	for (int i=0; i<app->tasks_number; i++){

		/*If task is not allocated to any processor*/
		if (app->tasks[i].allocated_proc == -1){

			t = &app->tasks[i];

			break;
		}
	}

	/*Means that the application has all its task already mapped*/
	if (t == 0){
		return 1;
	}


#if RECLUSTERING_DEBUG
		putsv("\nReclustering next task", t->id);
#endif

	/*fires reclustering protocol*/
	reclustering.active = 1;

	reclustering.task = t;

	reclustering.neighbors_level = 1;

	fires_reclustering_protocol();

	return 0;
}

#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_RECLUSTERING_H_ */
