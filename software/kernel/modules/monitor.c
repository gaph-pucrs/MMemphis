/*
 * low_level_monitor.c
 *
 *  Created on: Sep 19, 2019
 *      Author: ruaro
 */
#include "monitor.h"

#include "../../include/services.h"
#include "../../hal/mips/HAL_kernel.h"
#include "packet.h"
#include "utils.h"


unsigned int 	last_task_profiling_update;	//!< Used to store the last time that the profiling was sent to manager
unsigned int 	last_idle_time;				//!< Store the last idle time duration

unsigned int 	last_idle_time_report = 0;	//!< Store time at the last slack time update sent to manager
unsigned int 	total_idle_time = 0;		//!< Store the total of the processor idle time

void send_latency_miss(TCB * target_task, int producer_task, int producer_proc){

	unsigned int message[5];
	unsigned int master_addr, master_id;

	message[0] = LATENCY_MISS_REPORT;
	message[1] = target_task->id; //Consumer ID
	message[2] = net_address; //Producer ID
	message[3] = producer_task;
	message[4] = producer_proc;

	//Address of the local mapper
	master_addr = target_task->master_address & 0xFFFF;
	master_id = target_task->master_address >> 16;

	//puts("Sent task LATENCY_MISS_REPORT to "); puts(itoh(master_addr)); puts("\n");

	if (master_addr == net_address){

		//puts("Escrita local: send_task_terminated\n");

		write_local_service_to_MA(master_id, message, 5);

	} else {

		send_service_to_MA(master_id, master_addr, message, 5);

		//putsv("Master id: ", master_id);

		while(HAL_is_send_active(PS_SUBNET));
	}

}

/** Assembles and sends a SLACK_TIME_REPORT packet to the master kernel
 */
void send_slack_time_report(){

	return;

	unsigned int time_aux;

	ServiceHeader * p = get_service_header_slot();

	p->header = cluster_master_address;

	p->service = SLACK_TIME_REPORT;

	time_aux = HAL_get_tick();

	p->cpu_slack_time = ( (total_idle_time*100) / (time_aux - last_idle_time_report) );

	//putsv("Slack time sent = ", (total_idle_time*100) / (time_aux - last_idle_time_report));

	total_idle_time = 0;

	last_idle_time_report = time_aux;

	send_packet(p, 0, 0);
}

void init_profiling_window(){

	int app_ID, task_loc;

	app_ID = net_address >> 8;
	task_loc = net_address & 0xFF;
	app_ID += task_loc;

	//Rondomically defines the profiling update window according to the PE number, creating a heteroneus sorting
	last_task_profiling_update = (int)(APL_WINDOW/app_ID);

}

/* Inline because is called inside the scheduler
 */
inline void check_profiling_need(){

	 if(abs(last_idle_time-last_task_profiling_update) > APL_WINDOW){
		send_learned_profile(last_idle_time-last_task_profiling_update);
		last_task_profiling_update = last_idle_time;
	}
}

inline void reset_last_idle_time(){

	last_idle_time = HAL_get_tick();

}

inline void compute_idle_time(){

	total_idle_time += HAL_get_tick() - last_idle_time;
}
