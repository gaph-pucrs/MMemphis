/*
 * mapping.h
 *
 *  Created on: Jul 1, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_COMMON_INCLUDE_H_
#define APPLICATIONS_MAN_APP_COMMON_INCLUDE_H_

#include "../../include/kernel_pkg.h"
#include "../../software/include/services.h"
#include "../../software/include/stdlib.h"
#include <api.h>
#include <management_api.h>


#define LM_DEBUG 0
#define GM_DEBUG 0

//CAUTION: the size of XCLUSTER and YCLUSTER must be always divisible (remaining 0) with XDIMENSION and YDIMENSION

#define MAPPING_XCLUSTER		XCLUSTER
#define MAPPING_YCLUSTER		YCLUSTER
#define MAPPING_X_CLUSTER_NUM	(XDIMENSION/MAPPING_XCLUSTER)
#define MAPPING_Y_CLUSTER_NUM	(YDIMENSION/MAPPING_YCLUSTER)
#define MAX_MAPPING_TASKS		(MAPPING_X_CLUSTER_NUM*MAPPING_Y_CLUSTER_NUM)


#define SDN_XCLUSTER			XDIMENSION//XCLUSTER
#define	SDN_YCLUSTER			YDIMENSION//XCLUSTER
#define SDN_X_CLUSTER_NUM		(XDIMENSION/SDN_XCLUSTER)
#define SDN_Y_CLUSTER_NUM		(YDIMENSION/SDN_YCLUSTER)
#define	MAX_SDN_TASKS			(SDN_X_CLUSTER_NUM*SDN_Y_CLUSTER_NUM)

/* Create the macros for your MA task
#define MY_XCLUSTER			XCLUSTER
#define	MY_YCLUSTER			YCLUSTER
#define MY_X_CLUSTER_NUM		(XDIMENSION/MY_XCLUSTER)
#define MY_Y_CLUSTER_NUM		(YDIMENSION/MY_YCLUSTER)
#define	MAX_MY_TASKS			(MY_X_CLUSTER_NUM*MY_Y_CLUSTER_NUM)
*/


/*Others common defines*/

/* Defines the number where Packet-Switching subnet is refered in MMR*/
#define MAX_MANAG_MSG_SIZE		100

#define putsv(string, value) Puts(string); Puts(itoa(value)); Puts("\n");




typedef struct {

	unsigned int message[MAX_MANAG_MSG_SIZE];
	unsigned int status;

}MangmtMessageSlot;

volatile 		MangmtMessageSlot sdn_msg_slot1, sdn_msg_slot2;	//!<Slots to prevent memory writing while is sending a packet
unsigned int 	global_task_ID; 							//!< Stores the global mapper ID at task level
unsigned int 	S_task_ID_offset;								//!< Stores the offset of task IDs where tasks of same class of management starts
unsigned int 	S_num_x_cluster;
unsigned int 	net_address;


unsigned int * get_message_slot() {

	if ( sdn_msg_slot1.status ) {

		sdn_msg_slot1.status = 0;
		sdn_msg_slot2.status = 1;
		return (unsigned int *) &sdn_msg_slot1.message;

	} else {

		sdn_msg_slot2.status = 0;
		sdn_msg_slot1.status = 1;
		return (unsigned int *) &sdn_msg_slot2.message;
	}
}

void init_message_slots(){
    sdn_msg_slot1.status = 1;
    sdn_msg_slot2.status = 1;
}


void initialize_MA_task(){

	unsigned int * message;

	//Initializes global variables
	global_task_ID = 0;
	S_task_ID_offset = 0;
	S_num_x_cluster = 0;

	net_address = GetNetAddress();

	AddTaskLocation(global_task_ID,global_task_ID);

	message = get_message_slot();
	message[0] = INIT_I_AM_ALIVE;		//Task Service
	message[1] = net_address;	//Task Address
	message[2] = GetMyID();
	//Puts("Sending I AM ALIVE to global mapper\n");
	SendService(global_task_ID, message, 3);
}

/** Receives a cluster position 1x1, 2x1 e gives the index of the cluster
 *
 */
int conv_cluster_addr_to_task_ID(unsigned int cluster_addr, unsigned int x_cluster_num){
	int tx, ty;

	tx = cluster_addr >> 8;
	ty = cluster_addr & 0xFF;

	//Converts address in ID
	cluster_addr = (tx + ty*x_cluster_num) + 1; //PLus 1 because the global mapper uses ID 0

	return cluster_addr;
}

int conv_task_ID_to_cluster_addr(unsigned int task_id, unsigned int id_offset, unsigned int x_cluster_num){

	int tx, ty;

	task_id = task_id - id_offset;

	ty = task_id / x_cluster_num;
	tx = task_id - (ty*x_cluster_num);

	return ((tx << 8) | ty);

}

void init_generic_send_comm(unsigned int offset, unsigned int cluster_x_width){

	S_task_ID_offset = offset;
	S_num_x_cluster = (XDIMENSION / cluster_x_width);

	Puts("Generic send comm intialized\nS_task_ID_offset: "); Puts(itoa(S_task_ID_offset)); Puts("\nS_num_x_cluster: "); Puts(itoa(S_num_x_cluster)); Puts("\n");
}

/** This function abstract the communication among local mappers converting destination (0x1, 1x0 ...) to task IDs
 * It only works between the communication of task of the same class (betweeen two noc_manager tasks for example).
 * To communicate with other MA tasks use the SendService
 */
void send(unsigned int destination, unsigned int * msg, int msg_uint_size){
	int tx, ty;

	tx = destination >> 8;
	ty = destination & 0xFF;

	//Converts address in ID
	//if (!(destination & TO_KERNEL))//Only change the destination if it is not to kernel
	destination = tx + (ty*S_num_x_cluster) + S_task_ID_offset; //PLus 1 because the global mapper uses ID 0

	//Puts("Package sent to ID: "); Puts(itoa(destination)); Puts("\n");
	SendService(destination, msg, msg_uint_size);
}

/**Send a message to the SDN controller requesting a path request or path release
 * \param source_addr XY address of the source PE
 * \param target_addr XY address of the target PE
 * \param subnet -1 when is a PATH_REQUEST, otherwise, specifies the current subnet of the path
 */
void request_SDN_path(int source_addr, int target_addr, int subnet){
	int coordinator_task_ID;
	int src_x, src_y, nc_x, nc_y;
	int sdn_offset;
	unsigned int * send_message;

	src_x = source_addr >> 8;
	src_y = source_addr & 0xFF;


	nc_x = src_x / SDN_XCLUSTER;
	nc_y = src_y / SDN_YCLUSTER;

	sdn_offset = 2;//TODO Please edit when you add a new MA task 2

	coordinator_task_ID = nc_x + (nc_y*SDN_X_CLUSTER_NUM) + sdn_offset;

	//Puts("\nController addr is "); Puts(itoa(coordinator_task_ID)); Puts("\n");

	send_message = get_message_slot();
	if (subnet == -1){
		send_message[0] = PATH_CONNECTION_REQUEST;
	} else {
		send_message[0] = PATH_CONNECTION_RELEASE;
	}
	send_message[1] = source_addr; //source
	send_message[2] = target_addr; //target
	send_message[3] = GetMyID();
	send_message[4] = subnet;
	send_message[5] = net_address; //Secure SDN context: added to allows SDN manager check the addresse of manager

	SendService(coordinator_task_ID, send_message, 6);

	while(!NoCSendFree());



	//Puts("\nPath request "); Puts(itoh(source_addr)); Puts(" -> "); Puts(itoh(target_addr));  Puts("\n");
	//putsv("Start time: ", GetTick());
}


#endif /* APPLICATIONS_MAN_APP_COMMON_INCLUDE_H_ */
