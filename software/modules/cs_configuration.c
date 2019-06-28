/*
 * cs_configuration.c
 *
 *  Created on: 06/10/2016
 *      Author: mruaro
 */

#include "cs_configuration.h"
#include "../../include/kernel_pkg.h"
#include "../include/services.h"
#include "../include/plasma.h"
#include "../include/service_api.h"
#include "qos_self_adaptation.h"
#include "task_migration.h"
#include "resource_manager.h"
#include "processors.h"
#include "packet.h"
#include "utils.h"

ConnectionManager cs_manager;

ClusterInfo * cluster_i_ptr;

//global variables to manage runtime CS release and stablishment
CS_path * 		waiting_ctp = 0;
int 			waiting_cons_task = 0;
int				waiting_prod_task;


int search_controller_(int source_x, int source_y){
    unsigned int nc_x, nc_y;

    nc_x = source_x / XCLUSTER;
    nc_y = source_y / YCLUSTER;

    return (nc_x << 8 | nc_y);
}

/** External status check function
 */
inline int is_CS_NOT_active(){

	if (cs_manager.task_id == -1 && waiting_ctp == 0)
		return 1;

	return 0;
}

/** Cancell the CS configuration due a application finish
 */
void cancel_CS_reconfiguration(){
	cs_manager.task_id = -1;
}


/*
 * Aqui por enquanto o endereço do cluster pode ser descoberto de forma atifical
 * ou seja, apenas colocar endereço absoluto, criar uma matriz conhecendo onde cada controlador
 * sera mapeado
 */
unsigned int get_SDN_coordinator_address(int sourcePE){

	unsigned int nc_x, nc_y;

	/*Static definition of D-SDN*/
	unsigned int static_position[2][2];
    static_position[0][0] = 0x502;
    static_position[1][0] = 0x500;
    static_position[0][1] = 0x501;
    static_position[1][1] = 0x402;

    //return 0x502;//Centarlized Mode - Comment when in distributed

    nc_x = sourcePE >> 8;
    nc_y = sourcePE & 0xFF;

    nc_x = nc_x / XCLUSTER;
    nc_y = nc_y / YCLUSTER;

    puts("Manager add address ");
    puts(itoh(static_position[nc_x][nc_y]));
    puts(" to source PE ");
    puts(itoh(sourcePE));

    return static_position[nc_x][nc_y];

}

unsigned int get_SDN_coodinator_ID(int sourcePE){

	//return 0;//Centarlized Mode - Comment when in distributed

	unsigned int nc_x, nc_y;

	nc_x = sourcePE >> 8;
	nc_y = sourcePE & 0xFF;

	nc_x = (nc_x / XCLUSTER) * (XDIMENSION/XCLUSTER);
	nc_y = nc_y / YCLUSTER;

	return (nc_x + nc_y);
}


//******************** Functions that implement CS-Controller requests *************************

void CS_Controller_CS_request(int source_proc, int target_proc){

	ServiceHeader * p;
	volatile unsigned int data[4];

	data[0] = PATH_CONNECTION_REQUEST;
	data[1] = source_proc;
	data[2] = target_proc;
	data[3] = net_address;

	//Assembles a new packet
	p = get_service_header_slot();
	p->header = get_SDN_coordinator_address(source_proc);
	p->service = SERVICE_TASK_MESSAGE;
	p->consumer_task = get_SDN_coodinator_ID(source_proc);
	p->msg_lenght = 5;
	send_packet(p, (unsigned int)data, p->msg_lenght);

	while (is_send_active(PS_SUBNET));

	//puts(itoh(data[1])); puts(" -> "); puts(itoh(data[2])); puts("\n");
	sdn_setup_latency = MemoryRead(TICK_COUNTER);
	//putsv("\nRequested: ", sdn_setup_latency);

}

/** Sends to CS-Controller a release CTP order
 * \param source_proc The source processor address
 * \param target_proc The target processor address
 * \param subnet The CTP subnet
 * \param cs_path The connection path ID
 */
void CS_Controller_CS_release(int source_proc, int target_proc, int subnet, int cs_path){

	ServiceHeader * p;
	volatile unsigned int data[3];

	data[0] = PATH_CONNECTION_RELEASE;
	data[1] = source_proc;
	data[2] = target_proc;
	data[3] = net_address;
	data[4] = subnet;

	//Assembles a new packet
	p = get_service_header_slot();
	p->header = get_SDN_coordinator_address(source_proc);
	p->service = SERVICE_TASK_MESSAGE;
	p->consumer_task = get_SDN_coodinator_ID(source_proc);
	p->msg_lenght = 5;
	send_packet(p, (unsigned int)data, p->msg_lenght);

	while (is_send_active(PS_SUBNET));

}

/** Sends to CS-Controller a request to discovery the number of available connection of the check_processor DMNI
 * \param check_processor The processor address
 */
void CS_Controller_NI_status_check(int check_processor){

	ServiceHeader *p;
	volatile unsigned int data[3];

	//Assembles packet payload
	data[0] = NI_STATUS_REQUEST;
	data[1] = check_processor;
	data[2] = net_address;

	//Assembles a new packet
	p = get_service_header_slot();
	p->header = get_SDN_coordinator_address(check_processor);
	p->service = SERVICE_TASK_MESSAGE;
	p->consumer_task = get_SDN_coodinator_ID(check_processor);
	p->msg_lenght = 3;
	send_packet(p, (unsigned int)data, p->msg_lenght);

	while(is_send_active(PS_SUBNET));
	puts("Sending NI status check to CS-Controller, waiting ACK...\n");
}

//******************** END Functions that implement CS-Controller requests *************************








/** Send the SET_CS_CONSUMER packet to the kernel_slave of the consumer task of CTP.
 * \param producer_task Task ID of the consumer task of CTP
 * \param consumer_task Task ID of the consumer task of CTP
 * \param cs_mode Flag indicating to release or stablish a CS connection (1 - stablish, 0 - release)
 */
void send_set_CS_consumer(int producer_task, int consumer_task, int cs_mode, int subnet){

	ServiceHeader * p;

	p = get_service_header_slot();

	p->header = get_task_location(consumer_task);

	p->service = SET_NOC_SWITCHING_CONSUMER;

	p->producer_task = producer_task;

	p->consumer_task = consumer_task;

	p->allocated_processor = get_task_location(producer_task);

	p->cs_mode = cs_mode;

	p->cs_net = subnet;

	//puts("\nSend set consumer, waiting ACK...\n");

	/*puts("Send SET ctp to "); puts(itoa(producer_task)); putsv(" -> ", consumer_task);
	if(cs_mode)
		puts("\t in stablish CS mode\n");
	else
		puts("\t in release CS mode\n");
	 */

	send_packet(p, 0, 0);
}


/** Function that remove the CS allocation from the producer and consumer PE at the end of the task execution
 */
void send_clear_CS_ctp(int producer_task, int consumer_task, int cs_net){

	ServiceHeader *p = get_service_header_slot();

	//Send to producer
	p->header = get_task_location(producer_task);

	p->service = CLEAR_CS_CTP;

	p->producer_task = producer_task;

	p->consumer_task = consumer_task;

	p->dmni_op = DMNI_SEND_OP;

	p->cs_net = cs_net;

	send_packet(p, 0, 0);

	//putsv("Packet CLEAR_CS_CTP sent to PE ", p->header);
	//Send to consumer
	p = get_service_header_slot();

	p->header = get_task_location(consumer_task);

	p->service = CLEAR_CS_CTP;

	p->producer_task = producer_task;

	p->consumer_task = consumer_task;

	p->dmni_op = DMNI_RECEIVE_OP;

	p->cs_net = cs_net;

	send_packet(p, 0, 0);

	//putsv("Packet CLEAR_CS_CTP sent to header ", p->header);

}



//****************************** Functions used to sablishes a connection for a task on demand at runtime ***************************

void concludes_runtime_CS(int prod_task, int cons_task){

	if (prod_task == waiting_prod_task && cons_task == waiting_cons_task){

		putsv ("Online CS configuration finishes :) at time ", MemoryRead(TICK_COUNTER));

		//Clear structures after ack
		waiting_ctp = 0;
		waiting_cons_task = -1;
		waiting_prod_task = -1;

	} else {
		puts("ERROR - ACK, producer and cons different\n");
		while(1);
	}
}


/** Handle the CS-Controller ack to stablish CS connection on-demand.
 * This function set pathid and subnet information to the waiting_ctp and also send the set_cs_consumer order
 * to the CTP start to use the CS network
 *
 */
void set_runtime_CS(int path_id, int subnet){

	//puts("\n CS ack: ");

	if (waiting_ctp->id == waiting_cons_task){

		if (path_id != -1){

			puts(" connection stablished with CS path "); puts(itoa(path_id)); putsv(" subnet ", subnet);

			waiting_ctp->CS_path = path_id;

			waiting_ctp->subnet = subnet;

			//Clear miss
			clear_latency_and_deadline_miss(waiting_cons_task);

			//Send the order to the CTP start to use the CS network
			send_set_CS_consumer(waiting_prod_task, waiting_cons_task, 1, subnet);

		} else {

			puts(" connection UNAVALIABLE :(\n");

			waiting_ctp->CS_path = CS_NOT_AVAILABLE;

			waiting_ctp->subnet = -1;

			waiting_ctp = 0;
			waiting_cons_task = -1;
			waiting_prod_task = -1;
		}

	} else {
		puts("ERROR - Cons task not found\n");
		while(1);
	}

}


/** Start the on-the-fly CS stablishment protocoll
 */
void request_runtime_CS(int prod_task, int cons_task, int source_proc, int target_proc) {

	CS_path * ctp;

	if (waiting_ctp){
		puts("ERROR - requesting task not 0\n");
		while(1);
	}

	ctp = search_CS_path(prod_task, cons_task);

	if (ctp->CS_path != CS_PENDING){
		puts("\nERROR: CS status diferent from CS_PENDING - CS cancelled\n");
		while(1);
	}

	puts("\nRequesting CS for task: "); puts(itoa(prod_task)); puts(" [");puts(itoh(source_proc));puts("] -> "); puts(itoa(cons_task));puts(" [");puts(itoh(target_proc));puts("]\n");

	CS_Controller_CS_request(source_proc, target_proc);

	ctp->CS_path = WAITING_CS_ACK;

	waiting_ctp = ctp;
	waiting_prod_task = prod_task;
	waiting_cons_task = cons_task;

}

//**********************************************************************************************************************





/** Release all CS connection between the task_id and the producers task WHEN A TASK TERMINATES ITS EXECUTION
 * Note that only the connection that task_id is the consumer will be released
 *
 */
void release_task_CS(Application *app, int taks_id){

	Task *t;
	CS_path * ctp;
	int source_proc, target_proc;

	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		for (int c = 0; c < t->consumer_tasks_number; c++) {

			ctp = &t->consumer_tasks[c];

			//Ignores invalids ID and connection not stablished or local connections
			if (ctp->id != taks_id || ctp->CS_path <= 0) continue;

			source_proc = t->allocated_proc;

			target_proc = get_task_location(ctp->id);

			puts("-------------\nConnection Release\nSource = "); puts(itoh(source_proc));
			puts("\nTarget = "); puts(itoh(target_proc));
			puts("\nSubnet = "); puts(itoa(ctp->subnet));
			puts("\nPath_id = "); puts(itoa(ctp->CS_path)); puts("\n");

			//Release CS at CS-Controller
			CS_Controller_CS_release(source_proc, target_proc, ctp->subnet, ctp->CS_path);

			//Release CS at the source and target PE
			send_clear_CS_ctp(t->id, ctp->id, ctp->subnet);

			ctp->id = -1;

			ctp->subnet = -1;

			ctp->CS_path = CS_NOT_ALLOCATED;
		}
	}

}

/** This function is called by the manager in order to notifiies that CS_reconfiguration module that a
 * task migration was received. This module can test if the task migration notification is for the task that is
 * waiting for migration to proceed with the adaptation
 * \param task_id ID of the migrated task
 */
void task_migration_notification(int task_id){

	if (cs_manager.task_id == task_id){

		cs_manager.task_id = -1;

		putsv("ADPT FIM = ", MemoryRead(TICK_COUNTER));

		//Clear miss
		//clear_latency_and_deadline_miss(task_id);
		clear_app_deadline_miss(task_id>>8);

	}

}

/** Initialize the cs_manager structure with data related to the task ID.
 * \param task_id The task ID to be migrated
 */
void generate_connection_manager(int task_id){

	Application * app;
	Task *t;
	CS_path * ctp;

	app = get_application_ptr(task_id >> 8);

	cs_manager.task_id = task_id;
	cs_manager.ctp_size = 0;
	cs_manager.ack_counter = 0;
	cs_manager.connections_out = 0;
	cs_manager.connections_in = 0;

	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		for (int c = 0; c < t->consumer_tasks_number; c++) {

			ctp = &t->consumer_tasks[c];

			//if (ctp->CS_path <= 0) continue;
			if (ctp->id == CS_NOT_ALLOCATED) continue;

			//Test if the task belows to the ctp
			if ( (t->id == task_id || ctp->id == task_id) && t->status != TERMINATED_TASK && get_task_status(app, ctp->id) != TERMINATED_TASK){
				//Stores a next CTP, producer | consumer
				cs_manager.ctp_id[cs_manager.ctp_size] = t->id << 16 | ctp->id;
				cs_manager.ctp_pointer[cs_manager.ctp_size] = ctp; //Store the ctp pointer to speedup the access
				cs_manager.ctp_size++; //Increase the list

				if (t->id == task_id) //If the task is the producer
					cs_manager.connections_out++;
				else //If the task is the consumer
					cs_manager.connections_in++;

				//puts("Adicionou "); puts(itoa(cs_manager.ctp_id[cs_manager.ctp_size-1] >> 16)); putsv(" -> ", cs_manager.ctp_id[cs_manager.ctp_size-1] & 0xFFFF);
			}
		}
	}

}

/** Gets the number of CTPs if flag is_total_ctp is enabled, otherwise, returns percentage of CS connection established
 * \param task_id The task ID
 * \return A percentage value
 */
int get_CTP_number(int task_id, int is_total_ctp){
	Application * app;
	Task *t;
	CS_path * ctp;
	int total_in, total_out, total_connection_required, local_CTPs, connection_number, percent;

	local_CTPs = 0;
	connection_number = 0;
	total_in = 0;
	total_out = 0;
	total_connection_required = 0;

	app = get_application_ptr(task_id >> 8);

	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		for (int c = 0; c < t->consumer_tasks_number; c++) {

			ctp = &t->consumer_tasks[c];

			//if (ctp->CS_path <= 0) continue;
			if (ctp->id == -1) continue;

			//Test if the task belows to the ctp
			if (t->id == task_id || ctp->id == task_id){

				if (t->id == task_id) //If the task is the producer
					total_out++;
				else //If the task is the consumer
					total_in++;

				if (ctp->CS_path != LOCAL_ALLOCATION)
					total_connection_required++;

				if (ctp->CS_path > 0) //Counts only remote communications
					connection_number++;
			}
		}
	}

	if (is_total_ctp)
		return (total_in << 8 | total_out);

	//If total is 0 this mean that the function will returns the percentage of total connections
	percent = (connection_number*100)/total_connection_required;

	/*putsv("\nCTP Number = ", total_connection_required);
	putsv("Connections number = ", connection_number);
	putsv("Returned percentage = ", percent);*/

	return percent;
}

void release_CS_before_migration(int task_id, int selected_proc){

	CS_path * ctp;
	Application * app;
	int waiting_prod, waiting_cons, source_proc, target_proc, aux;
	static int candidate_proc = -1;

	if (cs_manager.task_id == -1) {//First call

		puts("\n------ Starting CS-Reconfiguration to target task "); puts(itoa(task_id)); puts(" ------ \n");

		//Fills the ConnectionManager Structure
		generate_connection_manager(task_id);

		//Get the next candidate PE
		candidate_proc = selected_proc;

		puts("Candidate PE = "); puts(itoh(candidate_proc)); puts("\n");

		//Start the ctp pointer with the first ctp available
		ctp = cs_manager.ctp_pointer[0];

		//Test if the connection is established for the first ctp. If yes, releases it.
		//And also test if the task communicates with something
		if (ctp->CS_path > 0 && cs_manager.ctp_size) {

			puts("\nReleasing Circuit-Switching...\n");

			send_set_CS_consumer(cs_manager.ctp_id[0] >> 16, cs_manager.ctp_id[0] & 0xFFFF, 0, 0);

			//Leaves the function and wait for the CS producer ack
			return;
		}
		//If the connection was not stablished goes to the QOS_RELEASE_CS_MODE case
		puts("\nReleasing Circuit-Switching...\n");


	} else {

		app = search_application(cs_manager.task_id >> 8);

		if (app == 0 || get_task_status(0, cs_manager.task_id) == TERMINATED_TASK){
			cs_manager.task_id = -1;
			putsv("ADPT SUSPENDED - task TERMINATED = ", MemoryRead(TICK_COUNTER));
			return;
		}

	}

	//At this point the CS producer ack will start to be handled

	//Sets aux to the next available ctp index pointing to cs_manager.ack_counter
	aux = cs_manager.ack_counter;

	//Get the ctp pointer
	ctp = cs_manager.ctp_pointer[aux];

	//Release the connection for the ctp
	if (ctp->CS_path > 0 && cs_manager.ctp_size) {//If the connection is not a local allocation

		//Gets the ID of the producer and consumer tasks
		waiting_prod = cs_manager.ctp_id[aux] >> 16;
		waiting_cons = cs_manager.ctp_id[aux] & 0xFFFF;

		//Gets the processor address
		source_proc = get_task_location(waiting_prod);
		target_proc = get_task_location(waiting_cons);

		//Release CS connection physically by requesting to CS-Controller
		CS_Controller_CS_release(source_proc, target_proc, ctp->subnet, ctp->CS_path);

		puts("ACK received - Sending release CS to CS-Controller - from task ");
		puts(itoa(waiting_prod)); puts("["); puts(itoh(source_proc)); puts("] -> "); puts(itoa(waiting_cons)); puts(" ["); puts(itoh(target_proc)); puts("]\n");

		//Set the CS structure to CS_NOT_ALLOCATED
		ctp->subnet = -1;

		ctp->CS_path = CS_NOT_ALLOCATED;

	} else {

		ctp->subnet = -1;

		ctp->CS_path = CS_NOT_ALLOCATED;
	}

	//Increment ack_counter, the index of the cs_manager array
	cs_manager.ack_counter++;

	//Search for the NEXT ctp with a connection established
	while(cs_manager.ack_counter < cs_manager.ctp_size && cs_manager.ctp_size){

		ctp = cs_manager.ctp_pointer[cs_manager.ack_counter];

		if (ctp->CS_path > 0) { //Only releases valid connections

			//If YES, start the release protocol for the new CTP
			waiting_prod = cs_manager.ctp_id[cs_manager.ack_counter] >> 16;
			waiting_cons = cs_manager.ctp_id[cs_manager.ack_counter] & 0xFFFF;

			send_set_CS_consumer(waiting_prod, waiting_cons, 0, 0);

			return;

		} else {
			cs_manager.ack_counter++;
		}

	}

	//At this point all CS connection are released, them request to the CS-Controller the
	//number of connection in and out available at the DMNI of the candidate proc
	//The response of this check will be handled by the QoS_Adaptation
	CS_Controller_NI_status_check(candidate_proc);

}

/** This function initializes the cs_manager structure and set the cluster_i_ptr pointer
 * \param cluster_id ID of the current cluster_info
 */
void init_CS_configuration(){

	cs_manager.task_id = -1;

	cluster_i_ptr = &cluster_info[clusterID];

}
