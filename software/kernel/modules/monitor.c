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

	ServiceHeader * p = get_service_header_slot();

	p->header = target_task->master_address;

	p->service = LATENCY_MISS_REPORT;

	p->producer_task = producer_task;

	p->consumer_task = target_task->id;

	p->target_processor = producer_proc;

	send_packet(p, 0, 0);

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
